#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <thread>
#include <mutex>

/* 128 KB */
#define MAX_DATA  131072

static std::mutex g_pos_mutex;
static std::mutex g_wsp_mutex;
static std::mutex g_sp_mutex;
static std::vector<char *> what_str_positions;
static off_t cur_ptr = 0;

off_t get_job(off_t filesize)
{
	off_t ret;
	std::lock_guard<std::mutex> lock(g_pos_mutex);
	ret 	= cur_ptr;
	if ( (cur_ptr != -1) && (cur_ptr < filesize) )
		cur_ptr = cur_ptr + MAX_DATA;
	else
		cur_ptr = -1;
	//std::cout << "Start position : " << ret << " thread id : " << std::this_thread::get_id() << std::endl;
	return ret;
}

unsigned int get_num_bytes(char *start, char *end)
{
	char *ptr = start;
	for(; ptr != end && *ptr != '\0' ; ptr++) {
		char c = *ptr;
		if (((c >= 0x20) && (c <= 0x7E)) || (c == 0x09))
			continue;
		else
			break;
	}
	return ptr - start;
}

bool has_newline(char *pos)
{
	unsigned int i;
	for (i=0; pos[i] != '\0' && pos[i] != '\n'; i++);
	if (pos[i] == '\0' && pos[i+1] == '\n') return true;
	if (pos[i] == '\n' && pos[i+1] == '\0') return true;
	return false;
}

void print_what_strings(char *end)
{
	std::sort (what_str_positions.begin(), what_str_positions.end());
	what_str_positions.erase( unique( what_str_positions.begin(),
										what_str_positions.end() ),
										 what_str_positions.end() );
	for(unsigned int i = 0; i != what_str_positions.size(); i++) {
		char *pos = what_str_positions[i];
		// skip @(#)
		pos = pos + 4;
		if (has_newline(pos))
			printf("\t%s", pos);
		else
			printf("\t%s\n", pos);
	}
	//std::cout << "Total : " << what_str_positions.size() << std::endl;
}

void store_what_str_position(char *what_pos)
{
	std::lock_guard<std::mutex> lock(g_wsp_mutex);
	what_str_positions.push_back(what_pos);
}

void search_what_strings(char *file_ptr, off_t start_pos, off_t filesize)
{
	char *begin = file_ptr + start_pos;
	char *end   = begin    + MAX_DATA;
	if (end > (file_ptr + filesize))
		end   = file_ptr + filesize;

	while (begin < end) {
		size_t size 	= end - begin;
		char *what_ptr  = (char *)std::memchr(begin, '@', size);
		if (NULL == what_ptr) return;
		if ( (*(what_ptr + 0) == '@') &&
			(*(what_ptr + 1) == '(') &&
			(*(what_ptr + 2) == '#') &&
			(*(what_ptr + 3) == ')') ) {
			store_what_str_position(what_ptr);
			begin = what_ptr + get_num_bytes(begin, end);
		} else begin = begin + 1;
	}
}

void worker(char *ptr, off_t filesize)
{
	off_t start_pos = -1;
	while (1) {
		start_pos = get_job(filesize);
		if (-1 == start_pos) return;
		search_what_strings(ptr, start_pos, filesize);
	}
}

void print_output(char *filename, char *ptr, off_t filesize)
{
	printf("%s:\n", filename);
	print_what_strings(ptr+filesize);
}

void process_input_file(char *filename, off_t filestart, off_t filesize)
{
	//std::cout << "File size : " << filesize << std::endl;
	int fd = open(filename, O_RDONLY);
	if(-1 == fd) {
		std::cout << "Error: Failed to open file : " << filename << " : " << strerror(errno) << std::endl;
		exit(1);
	}
	char *ptr = (char *)mmap(0, filesize, PROT_READ, MAP_SHARED, fd, filestart);
	if (ptr == MAP_FAILED) {
		std::cout << "Error: Failed to mmap file : " << filename << " : " << strerror(errno) << std::endl;
		exit(1);
	}

	// Create more than one thread only if filesize > MAX_DATA
	unsigned int num_jobs = 1;
	if (filesize > MAX_DATA) num_jobs    = filesize / MAX_DATA;
	//std::cout << "Number of possible jobs : " << num_jobs << std::endl;
	unsigned int num_threads = std::thread::hardware_concurrency();

	// make sure we habe enough data for all threads
	if (num_threads > num_jobs) num_threads = num_jobs;

	//std::cout << "Number of threads : " << num_threads << std::endl;
	std::vector<std::thread> thread_objs(num_threads);

	/* Create threads */
	for (unsigned int i=0; i<num_threads; i++) {
		thread_objs[i] = std::thread(worker, ptr, filesize);
	}
	/* Wait for all threads to complete */
	std::for_each(thread_objs.begin(), thread_objs.end(), [](std::thread &th) {
						th.join();
					}
				);
	print_output(filename, ptr, filesize);
	munmap(ptr, filesize);
	close(fd);
}

static void usage(char *binary) {
	(void)fprintf(stderr, "usage: %s <options> file ...\n", binary);
	(void)fprintf(stderr, "      -o file offset to start search from\n");
	(void)fprintf(stderr, "      -n number of bytes to be searched\n");
	exit(1);
}

bool is_str_digit(const char *str)
{
	unsigned int i;
	if (str) {
		for (i=0; str[i] != '\0'; i++)
			if (!isdigit(str[i])) return false;
	} else return false;
	return true;
}

void parse_options(int argc, char *argv[], off_t *off, off_t *len)
{
	int c;
	while ((c = getopt(argc, argv, "o:n:")) != -1) {
		switch (c) {
			case 'o':
				/* TODO: Check this can be done with _FILE_OFFSET_BITS */
				if (sizeof(off_t) != 8) {
					std::cout << "Error: '-o' option not supported on this machine" << std::endl;
					exit(1);
				}
				if (is_str_digit(optarg))
					*off = strtoll(optarg, NULL, 10);
				else {
					std::cout << "Error: '-o' option accepts only numeric value" << std::endl;
					exit(1);
				}
				break;

	  		case 'n':
	  			if (is_str_digit(optarg))
					*len = strtoll(optarg, NULL, 10);
				else {
					std::cout << "Error: '-n' option accepts only numeric value" << std::endl;
					exit(1);
				}
				break;
	  		default:
				usage(argv[0]);
	  	}
	}
}

int main(int argc, char *argv[])
{
	off_t start 		 = 0;
	off_t end 			 = LLONG_MAX;
	parse_options(argc, argv, &start, &end);
	argv += optind;
	char *filename 		 = argv[0];
	if (filename) {
		struct stat st;
		if (stat(filename, &st) != 0) {
			std::cout << "Error: Failed to open file " << filename << std::endl;
			exit(1);
		} else {
			if (LLONG_MAX == end) end = st.st_size;
			else if (end < 0) {
				end = st.st_size;
			} else if (0 == end) {
				return 0;
			} else if (end > st.st_size) {
				end = st.st_size;
			}

			if (start >= st.st_size) start = 0;
			else if (start < 0) start = 0;

			process_input_file(filename, start, end);
		}
	} else {
		std::cout << "Error: Supply file name as argument" << std::endl;
		exit(1);
	}
	return 0;
}
