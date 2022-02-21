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
#include <string.h>
#include <limits.h>

/* 128 KB */
#define MAX_DATA  131072
#define MAX_MAP_SIZE 1024

static std::mutex g_pos_mutex;
static std::mutex g_wsp_mutex;
static std::mutex g_sp_mutex;
static std::vector<char *> what_str_positions;
static std::map<char *, std::string> str_positions;
static off_t cur_ptr = 0;
static struct stat st;
static int found  = 0;

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

void print_strings()
{
  for (auto i : str_positions)
    std::cout << i.second << std::endl;
}

void store_str_position(char *begin, char *b)
{
  std::lock_guard<std::mutex> lock(g_sp_mutex);
  str_positions.insert(std::make_pair(begin, std::string(b)));
  if (str_positions.size() > MAX_MAP_SIZE) {
    print_strings();
    str_positions.clear();
  }
}

#define STRINGS_MAX 1024
#define GNU_STRINGS_COMPAT
#define STRINGS_MIN 9
void search_strings(char *file_ptr, off_t start_pos, off_t filesize)
{
  char b[STRINGS_MAX];
  b[STRINGS_MAX-1] = '\0';
  size_t i = 0;
  int dump = 0;
  int c;
  char *begin = file_ptr + start_pos;
  char *end   = begin    + MAX_DATA;
  if (end > (file_ptr + filesize))
    end   = file_ptr + filesize;

  while (begin < end) {
    c = *begin;
    if (c == 0) {
      if (!dump && i > STRINGS_MIN) {
	b[i] = 0;
	//puts(&b[0]);
        store_str_position(begin, &b[0]);
      }
      i = 0;
      dump = 0;
    } else if (((c >= 0x20) && (c <= 0x7E)) || (c == 0x09)) {
      b[i] = c;
      i++;
      if (i == STRINGS_MAX-1) {
	i = 0;
	dump = 1;
      }
    } else {
#ifdef GNU_STRINGS_COMPAT
      /* also catches non null terminated strings */
      if (!dump && i > STRINGS_MIN) {
	b[i] = 0;
	//puts(&b[0]);
        store_str_position(begin, &b[0]);
      }
#endif
      i = 0;
      dump = 0;
    }
    begin++;
  } /* while */
}

bool has_newline(std::string pos, off_t &index)
{
  unsigned int i;
  for (i=0; pos[i] != '\0' && pos[i] != '\n'; i++);
  if ((pos[i] == '\0' && pos[i+1] == '\n') || (pos[i] == '\n')) {
     index = i;
     return true;
  }
  return false;
}

void print_what_strings(char *end)
{
	std::sort (what_str_positions.begin(), what_str_positions.end());
	what_str_positions.erase( unique( what_str_positions.begin(),
										what_str_positions.end() ),
										 what_str_positions.end() );
  off_t index;
  for(unsigned int i = 0; i != what_str_positions.size(); i++) {
    std::string pos = what_str_positions[i];
    // skip @(#)
    index = 0;
    pos = pos.substr(4,-1);
    if (has_newline(pos, index)) {
       if (index > 0)
          pos= pos.substr(0,index + 1);
      std::cout<<"\t"<<pos;
      found = 1;
    } else {
      std::cout<<"\t"<<pos<<std::endl;
      found = 1;
    }
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
		char *what_ptr  = (char *)memchr(begin, '@', size-1);
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

void print_output(char *ptr, off_t filesize)
{
    print_what_strings(ptr+filesize);
}

/* As per mmap manual
   offset must be a multiple of the page size as
   returned by sysconf(_SC_PAGE_SIZE)
*/
off_t make_page_aligned(off_t filestart)
{
	if (0 == filestart) return filestart;
	off_t aligned = filestart;
	off_t diff    = 0;
#ifdef __linux__
	long page_sz = sysconf(_SC_PAGESIZE);
#elif __FreeBSD__
	int page_sz  = getpagesize();
#endif
	//std::cout << "Page size : " << page_sz << std::endl;
	if (filestart < page_sz) aligned = 0;
	else {
		diff 		= filestart % page_sz;
		aligned 	= filestart - diff;
	}
	//std::cout << "Start aligned : " << aligned  << " diff : " << diff << std::endl;
	return aligned;
}

void process_input_file(char *filename, off_t filestart, off_t filesize)
{
	//std::cout << "File size : " << filesize << std::endl;
	int fd = open(filename, O_RDONLY);
	if(-1 == fd) {
		std::cout << "Error: Failed to open file : " << filename << " : " << strerror(errno) << std::endl;
		exit(1);
	}
	off_t aligned_filestart = make_page_aligned(filestart);
	off_t aligned_diff 		= 0;
	off_t mmap_filesize 	= filesize;
	if (aligned_filestart != filestart) {
		aligned_diff 		= filestart - aligned_filestart;
    mmap_filesize    	= filesize + aligned_diff;
    if (aligned_filestart + mmap_filesize > st.st_size) {
      mmap_filesize  = st.st_size - aligned_filestart;
		}
	}

	char *aligned_ptr = (char *)mmap(0, mmap_filesize, PROT_READ, MAP_SHARED, fd, aligned_filestart);
	if (aligned_ptr == MAP_FAILED) {
		std::cout << "Error: Failed to mmap file : " << filename << " : " << strerror(errno) << std::endl;
		exit(1);
	}

	char *start_ptr = aligned_ptr + aligned_diff;
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
		thread_objs[i] = std::thread(worker, start_ptr, filesize);
	}
	/* Wait for all threads to complete */
	std::for_each(thread_objs.begin(), thread_objs.end(), [](std::thread &th) {
						th.join();
					}
				);
	print_output(start_ptr, filesize);
	munmap(aligned_ptr, mmap_filesize);
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
			// start & end can't be negative, -o & -v option throws error
			if (start > st.st_size) start = 0;
			if (LLONG_MAX == end) {
				if (start == 0)
					end = st.st_size;
				else
					end = st.st_size - start;
			} else if (0 == end) {
				return 0;
			} else if (end > st.st_size) {
				if (start == 0)
					end = st.st_size;
				else
					end = st.st_size - start;
			}

			process_input_file(filename, start, end);
		}
	} else {
		std::cout << "Error: Supply file name as argument" << std::endl;
		exit(1);
	}
	return 0;
}
