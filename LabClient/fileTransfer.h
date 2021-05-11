#ifndef _FILETRANSFER_H
#define _FILETRANSFER_H

//#include <iostream>
//#include <string.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <sstream>
//#include <vector>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <unistd.h>
#include <fstream>
#include <iomanip>
#include <unordered_map>

#ifdef _WIN32
#include <sstream>
#include <stdio.h>
#include <Windows.h>
#include <Wincrypt.h>

#define BUFSIZE 1024
#define MD5LEN  16
#endif

#ifdef __linux__
#include <openssl/md5.h>
#include <dirent.h>
#endif

#include "networking.h"

//Data header chunk size
#define CHUNK_SIZE 1024

//Forward declaration of filestats so we can use it in fileparts
struct FILESTATS;

//Holds stats about an individual data chunk
struct FILEPARTS {
    int chunk_id;
    int full_size;
    int data_size;
    char * data;
    char * full_header;
    std::string md5;
    std::string super_md5;
    FILESTATS * stats;

    ~FILEPARTS() {
        //delete[] data; //deleted during execution
        //delete[] full_header;
        printf("FILEPARTS DELETED: %s\n", md5.c_str());
    }
};

//Holds stats about the current file being downloaded
struct FILESTATS {
    int size;
    int chunk_size;
    int parts_number;
    //std::vector<FILEPARTS*> parts;
    //Map of currently in-progress downloads
    std::unordered_map<int, FILEPARTS*> parts;
    std::string md5;
    std::string file; //can potentially hold full path, mostly used on sending side
    std::string filename; //can only hold file names, mostly used on receiving side

    ~FILESTATS() {
        //Free up our memory
        printf("FILESTATS DELETED: %s\n", md5.c_str());
    }
};

int parseFile(MoonNetworking::Connection *c, char * buf, int length);
std::string md5_file(std::string path);
std::string md5(unsigned char * buffer, int length);
std::string build_file_header(std::string file, int chunk_size, FILESTATS &f);
int calculate_chunk_number(std::string file) ;
int get_file_size(std::string file);
unsigned char * get_chunk_data(std::string file, int chunk_number, int &data_size);
char * build_chunk_header(std::string file, std::string file_md5, int chunk_number, FILEPARTS &f);
void send_chunk_patch(MoonNetworking::Connection *c, std::string file, std::string super_md5, int chunk_number);
int send_file(MoonNetworking::Connection *c, std::string file);
int append_to_file(std::string file, char * buf, int length);
FILEPARTS * get_chunk_from_header(char * header, int length);
FILESTATS * get_super_header(char * header);
int write_chunk_to_file(std::string file, char * buf, int length, int chunk_number, int chunk_size);
void end_file_transmission(MoonNetworking::Connection * c, char*);
void send_end_file(MoonNetworking::Connection * c, std::string md5);
bool check_connection(MoonNetworking::Connection * c);
void init_file(std::string);
std::vector<int> verify_chunks(std::string, std::string);
std::vector<std::string> build_patch_requests(std::vector<int> missing_chunks, int chunk_size, std::string md5);
void parse_chunk_patch_request(MoonNetworking::Connection*, char*);
std::string list_files();
#endif