#include "utils.h"
#include "fileTransfer.h"

int last_id = 0;
bool done = false;

//Map of currently in-progress downloads
std::unordered_map<std::string, FILESTATS*> statlist;

int parseFile(MoonNetworking::Connection *c, char * buf, int length) {
    //Ensure we don't waste processing time if it's not a file
    if (strncmp(buf, "/file", 5) != 0) {
        return 0;
    }
    if (strncmp(buf, "/file-data:", 11) == 0) {
        if (strncmp(buf, "/file-data:start:", 17) == 0) {
            try {
                FILESTATS * f = get_super_header(buf);
                statlist[f->md5] = f;
                f->file = f->filename; //if we're receiving, we're storing the
                                       //file in the cwd anyway, so there's no
                                       //reason not to set this here
                if (file_exists(f->filename)) {
                    remove(f->filename.c_str());
                }
            }
            catch (int) {
                printf("INVALID START HEADER DATA\n");
            }
        }
        else if (strncmp(buf, "/file-data:part:", 16) == 0) {
            try {
                FILEPARTS * f = get_chunk_from_header(buf, length);

                //Flub some chunks
                //int maybe = rand() % 3;
                //printf("%d %d\n", time(0), maybe);
                /*
                if (maybe != 0) {
                    write_chunk_to_file(f->stats->filename, f->data, f->data_size, f->chunk_id, f->stats->chunk_size);
                }
                *?

                /*
                if ((f->chunk_id != 2) && (f->chunk_id != 3) && (f->chunk_id != 4) && (f->chunk_id != 6) && (f->chunk_id != 9)) {
                    write_chunk_to_file("received", f->data, f->data_size, f->chunk_id, CHUNK_SIZE);//f->stats->chunk_size);
                }
                */
                
                /*
                if (f->chunk_id != 3 || done) {
                    write_chunk_to_file("received", f->data, f->data_size, f->chunk_id, CHUNK_SIZE);//f->stats->chunk_size);
                }
                else {
                    done = true;
                    printf("Skipping: %d\n", f->chunk_id);
                }
                */
                write_chunk_to_file(f->stats->filename, f->data, f->data_size, f->chunk_id, f->stats->chunk_size);

                if (last_id != f->chunk_id - 1) {
                    //printf("CHUNK QUESTION %d, %d\n", last_id, f->chunk_id);
                }
                last_id = f->chunk_id;

                //Free up what we won't be using anymore
                delete[] f->data;
                f->data = 0;
            }
            catch (int) {
                printf("INVALID HEADER DATA\n");
            }
        }
        else if (strncmp(buf, "/file-data:request:", 19) == 0) {
            printf("req: %s\n", buf);
            parse_chunk_patch_request(c, buf);
        }
        else if (strncmp(buf, "/file-data:end:", 15) == 0) {
            end_file_transmission(c, buf);
        }
    }
    else if (strncmp(buf, "/file-req", 9) == 0) {
        send_file(c, &buf[10]);
    }
    else if (strncmp(buf, "/file-list", 10) == 0) {
        //c->sendTCP(list_files());
    }
    else if (strncmp(buf, "/file-notification", 18) == 0) {
        printf("%s\n", &buf[19]);
        char e[100];
        strcpy(e, &buf[19]);
    }
    return 1;
}

//High-level function to send a file.
//First sends the file info through tcp, then
//data through udp. If the Connection is missing
//a tcp or udp connection, returns 1
int send_file(MoonNetworking::Connection *c, std::string file) {
    if (!check_connection(c)) {
        printf("Client not connected with both TCP and UDP\n");
        return 1;
    }
    //printf("SENDING: \"%s\"\n", file.c_str());

    FILESTATS * f = new FILESTATS;
    c->sendTCP(build_file_header(file, CHUNK_SIZE, *f));
    statlist[f->md5] = f; //Store the currently sending file
    f->file = file;
    for (int i = 0; i < calculate_chunk_number(file); i++) {
        send_chunk_patch(c, file, f->md5, i);
    }
    send_end_file(c, f->md5);
    printf("SENT.\n");
	return 0;
}

//This function takes a buf value and
//parses it into the necessary md5. From there
//we can pick out the FILESTATS structure from the
//unordered map statlist and use it to verify
//the integrity of the received file.
//If there's integrity issues, we'll build
//a list of chinks to retransmit and do the
//send it. This function will be called again after
//the retransmission.
void end_file_transmission(MoonNetworking::Connection * c, char * buf) {
    char * md5 = &buf[16];
    char * bracket = strchr(md5 + 1, '}');
    int md5_size = bracket - md5;

    char md5_s[33];
    memset(md5_s, 0, 33);
    strncpy_s(md5_s, md5, md5_size);
    
    //printf("TRUE MD5: %s\n", md5_s);

    if (!statlist[md5_s]) {
        printf("Hey-ho! It's segfault 2 here to ruin your day!\n");

        printf("...BUT WAIT! Your valiant Knight, \"Shitty code\", is\nhere to save the day! --again!\n");

        while (!statlist[md5_s]) {
            Sleep(10);
        }
    }

    std::string current_md5 = md5_file(statlist[md5_s]->file);
    if (current_md5 == md5_s) {
        printf("FILE VERIFIED.\n");
        std::string m = "/file-notification FILE RECEIVED AND VERIFIED.";
        c->sendTCP(m);
        
        //Since we're verified, we don't need all those
        //nasty structures anymore
        //free(statlist[md5_s]->parts);
    }
    else {
        printf("FILE HAS MD5: %s\n", current_md5.c_str());

        //Send each patch request we generate
        std::vector<int> nums = verify_chunks(statlist[md5_s]->file, md5_s);
        std::vector<std::string> reqs = build_patch_requests(nums, statlist[md5_s]->chunk_size, md5_s);

        for (unsigned int i = 0; i < reqs.size(); i++) {
            //printf("REQ: %s\n", s.c_str());
            std::string s = reqs.at(i);
            c->sendTCP(s);
        }
    }
}

//Parse a patch request and send the patches
void parse_chunk_patch_request(MoonNetworking::Connection * c, char * buf) {
    char * md5 = &buf[19];
    char * end_colon = strchr(md5, ':');
    char * start = strchr(md5, '{');
    start++; //Skip the nasty bracket
    char * bracket = strchr(start + 1, '}');

    //If there's another colon in here, there's an end tag
    bool end = false;
    if (end_colon != 0) end = true;

    char * md5_s = new char[start - md5];
    md5_s[start - md5 - 1] = '\0';
    strncpy_s(md5_s, start - md5, md5, start - md5 - 1);

    //printf("MD5: %s\n", md5_s);
    //printf("START %s\n", start);
    
    char * current_numbers = start;
    char * comma = strchr(start, ',');
    //There won't be commmas if there's only one chunk to patch
    if (comma == 0) {
        comma = bracket;
    }
    //printf("BCL %s\n", buf);
    while (comma != 0) {
        //printf("LOOP %s\n", current_numbers);
        //Memchr so we can control the number of characters to search
        //If this is a range entry
        char * dash = (char*)memchr(current_numbers, '-', comma - current_numbers);
        if (dash != 0) {
            char * lowbound = new char[dash - current_numbers + 1]; //+1 for null termination
            memcpy(lowbound, current_numbers, dash - current_numbers);
            lowbound[dash - current_numbers] = '\0';

            current_numbers = dash + 1;
            //printf("CurrentNm: %s\n", current_numbers);
            char * highbound = new char[comma - current_numbers + 1]; //+1 for null termination
            memcpy(highbound, current_numbers, comma - current_numbers);
            highbound[comma - current_numbers] = '\0';
            //printf("Lowbound : Highbound ::: %s : %s \n", lowbound, highbound);

            for (int i = atoi(lowbound); i <= atoi(highbound); i++) {
                send_chunk_patch(c, statlist[md5_s]->file, md5_s, i);
            }
        }
        //Otherwise, if this is a single entry
        else {
            char * specific = new char[comma - current_numbers + 1]; //+1 for null termination
            memcpy(specific, current_numbers, comma - current_numbers);
            specific[comma - current_numbers] = '\0';
            //printf("SINGLE: %d\n", atoi(specific));
            send_chunk_patch(c, statlist[md5_s]->file, md5_s, atoi(specific));
        }
        current_numbers = comma + 1;
        //If it's still possible to find a comma...
        if (comma < bracket) comma = strchr(comma + 1, ',');
        //If there's no more commas, try a bracket
        if (comma == 0 && comma != bracket) {
            comma = bracket;
        }
        //If we just did the above, it's time to end
        else if (comma == bracket) comma = 0;
    }
    
    //In the case of multiple patch requests, the last one
    //will have the end tag. Once we're done with it, let the
    //client know.
    if (end) {
        send_end_file(c, md5_s);
    }
}

//Returns a vector of strings to send as patch requests.
//There can potentially be a LOT of requests, so we need to
//split them up by (chunk size) to be safe.
std::vector<std::string> build_patch_requests(std::vector<int> missing_chunks, int chunk_size, std::string md5) {
    std::vector<std::string> requests;

    std::ostringstream builder;
    builder << "/file-data:request:" << md5 << "{";
    std::vector<int> stored_chunks;
    int previous_chunk = missing_chunks.at(0); //Take first missing chunk as the prev value
    //Here we'll read the numbers into sections
    //ie: 0, 2, 5, 6-10, 14    
    for (int i : missing_chunks) {
        //If we're nearing the upper limit of the chunk size,
        //split this off into a new section
        //TODO: Make this more accurate
        if (builder.str().length() > (unsigned int)chunk_size - 20) {
            //Replace the last comma with a bracket
            std::string s = builder.str();
            s.replace(s.length() - 1, 1, "}");
            requests.push_back(s);
            //Clear and reset the stringstream
            builder.str(std::string());
            builder << "/file-data:request:" << md5 << "{";
        }

        //If there's a gap between missing chunks, but
        //a straight of them behind us
        if ((previous_chunk + 1) < i) {
            if (stored_chunks.size() == 0) {
                stored_chunks.push_back(i);
            }
            else if (stored_chunks.size() == 1) {
                builder << stored_chunks.back() << ",";
                stored_chunks.pop_back();
                stored_chunks.push_back(i);
            }
            else if (stored_chunks.size() > 1) {
                builder << stored_chunks.at(0) << "-" << stored_chunks.at(stored_chunks.size() - 1) << ",";
                stored_chunks.clear();
                stored_chunks.push_back(i);
            }
        }
        //If this is the next in a series of missing chunks
        else if ((previous_chunk + 1) == i) {
            stored_chunks.push_back(i);
        }
        //Otherwise, we're probably at the beginning of the parsing
        else {
            stored_chunks.push_back(i);
        }
        previous_chunk = i;
    }

    //Ugly, but here we deal with any un-inserted chunks after the loop
    //That'll end up happening because it's a foreach loop
    if (stored_chunks.size() == 1) {
        builder << stored_chunks.back();
        stored_chunks.pop_back();
    }
    else if (stored_chunks.size() > 1) {
        builder << stored_chunks.at(0) << "-" << stored_chunks.at(stored_chunks.size() - 1);
        stored_chunks.clear();
    }

    builder << "}:"; // <-- The colon signifies the last request to be sent

    //printf("REQUEST: %s\n", builder.str().c_str());
    
    requests.push_back(builder.str());
    return requests;
}

//Verify each individual piece of a file and return a list
//of invalid chunks
std::vector<int> verify_chunks(std::string file, std::string super_md5) {
    std::vector<int> missing;
    unsigned char * buffer = new unsigned char[statlist[super_md5]->chunk_size];
    std::ifstream input_file(file.c_str(), std::ifstream::binary);

    int i = 0;
    while (input_file.read((char *)buffer, statlist[super_md5]->chunk_size))
    {
        //If we don't even have the chunks yet it's obvious they're missing
        /*
        if (!statlist[super_md5]->parts[i] == 0) {
            printf("%d : \"%s\" : \"%s\"", i, statlist[super_md5]->parts[i]->md5.c_str(), md5(buffer, input_file.gcount()).c_str());
        }
        else printf("[EMPTY] : [EMPTY]");
        */
        if (statlist[super_md5]->parts[i] == 0) {
            missing.push_back(i);
        }
        else if (md5(buffer, (int)input_file.gcount()) != statlist[super_md5]->parts[i]->md5) {
            missing.push_back(i);
            //printf(" <--");
        }
        //printf("\n");
        i++;
        memset(buffer, 0, statlist[super_md5]->chunk_size);
    }
    input_file.close();

    //If the file isnt big enough, we haven't even seen the last
    //chunk yet! Let's add an entry for all the rest of the missing
    //stuff.
    if (statlist[super_md5]->parts_number > i) {
        for (; i < statlist[super_md5]->parts_number; i++) {
            missing.push_back(i);
        }
    }

    return missing;
}

//Sends a simple end file transmission
void send_end_file(MoonNetworking::Connection * c, std::string md5) {
    std::ostringstream ret;
    ret << "/file-data:end:{";
    ret << md5;
    ret << "}";

    c->sendTCP(ret.str().c_str());
}

void send_chunk_patch(MoonNetworking::Connection *c, std::string file, std::string file_md5, int chunk_number) {
    //printf("Sending: %d\n", chunk_number);
    FILEPARTS f;
    char * header = build_chunk_header(file, file_md5, chunk_number, f);
    c->sendUDP(header, f.full_size);

    //Free up pointers we won't need
    delete[] f.data;
    delete[] f.full_header;
}

//Takes a file name, desired chunk size, and FILESTATS structure
//Returns a string suitable to send as a file info header
std::string build_file_header(std::string file, int chunk_size, FILESTATS &f) {
    f.md5 = md5_file(file);
    f.size = get_file_size(file);
    f.parts_number = calculate_chunk_number(file);
    
    std::string filename = file;
    std::size_t pos = filename.find("/");
    if (pos != std::string::npos) {
        filename = filename.substr(pos + 1);
    }
    f.filename = filename;

    std::ostringstream ret;
    ret << "/file-data:start:{";
    ret << f.filename;
    ret << ":";
    ret << f.md5;
    ret << ":";
    ret << f.parts_number;
    ret << ":";
    ret << chunk_size;
    ret << ":";
    ret << f.size;
    ret << "}";

    return ret.str();
}

//Takes a filename to read from, md5 of the file, desired chunk number,
//and a reference to a FILEPARTS structure.
//Returns a set of bytes suitable to send as a specific chunk data
//Also fills out necessary info in the FILEPARTS structure
char * build_chunk_header(std::string file, std::string file_md5, int chunk_number, FILEPARTS &f) {
    int data_size = 0;
    unsigned char * data = get_chunk_data(file, chunk_number, data_size);
    
    std::ostringstream ret;
    ret << "/file-data:part:";
    ret << chunk_number;
    ret << ":{";
    ret << file_md5;
    ret << ":";
    ret << md5(data, data_size);
    ret << ":";

    int size = data_size + strlen(ret.str().c_str()) + 2; //"2" is for the "}\0" that we'll be appending

    unsigned char * header  = new unsigned char[size];
    memcpy(header, ret.str().c_str(), strlen(ret.str().c_str())); //+1 for null terminator

    int current_pos = strlen(ret.str().c_str());

    memcpy(header + current_pos, data, data_size);

    current_pos += data_size;

    memcpy(header + current_pos, "}", strlen("}") + 1); //+1 for null terminator
    
    if (&f != 0) {
        f.full_size = size;
        f.data_size = data_size;
        f.md5 = md5(data, data_size);
        f.data = (char*)data;
        f.full_header = (char*)header;
        f.chunk_id = chunk_number;
    }
    return (char *)header;
}

//Parses a file info header and returns a FILESTATS structure
FILESTATS * get_super_header(char * header) {
    char * filename = &header[18]; //Check usage for magic number explanation
    char * md5 = strchr(filename, ':');
    char * parts = strchr(md5 + 1, ':');
    char * chunk_size = strchr(parts + 1, ':');
    char * size = strchr(chunk_size + 1, ':');
    char * bracket = strchr(size + 1, '}');

    int size_size = (bracket - 1) - size;
    int chunk_size_size = (size - 1) - chunk_size;
    int parts_size = (chunk_size - 1) - parts;
    int md5_size = (parts - 1) - md5;
    int filename_size = md5 - filename; //Christ, just don't ask.

    char * filename_s = new char[filename_size + 1];
    filename_s[filename_size] = '\0'; //Null terminate the string
    strncpy_s(filename_s, filename_size + 1, filename, filename_size);

    char * md5_s = new char[md5_size + 1];
    md5_s[md5_size] = '\0'; //Null terminate the string
    strncpy_s(md5_s, md5_size + 1, md5 + 1, md5_size);

    char * parts_s = new char[parts_size + 1];
    parts_s[parts_size] = '\0';
    strncpy_s(parts_s, parts_size + 1, parts + 1, parts_size); //'parts + 1' to avoid the ':' character

    char * chunk_size_s = new char[chunk_size_size + 1];
    chunk_size_s[chunk_size_size] = '\0';
    strncpy_s(chunk_size_s, chunk_size_size + 1, chunk_size + 1, chunk_size_size); //'chunk_size_size + 1' to avoid the ':' character

    char * size_s = new char[size_size + 1];
    size_s[size_size] = '\0';
    strncpy_s(size_s, size_size + 1, size + 1, size_size); //'size + 1' to avoid the ':' character

    //Build resulting filestats pointer and return
    FILESTATS * f = new FILESTATS;
    f->size = atoi(size_s);
    f->chunk_size = atoi(chunk_size_s);
    f->md5 = std::string(md5_s); //Ensure we only get what we need
    f->parts_number = atoi(parts_s);
    f->filename = filename_s;
    return f;
}

//Parses a data header and returns a FILEPARTS structure
FILEPARTS * get_chunk_from_header(char * header, int length) {
    char * chunk_id = &header[16]; //Check usage for magic number explanation
    char * info_start = strchr(chunk_id, ':'); //There's another colon after the part id, so...
    char * super_md5 = strchr(info_start + 1, '{');
    char * md5 = strchr(super_md5 + 1, ':');
    char * data = strchr(md5 + 1, ':');

    int md5_size = (data - 1) - md5;
    int super_md5_size = (md5 - 1) - super_md5;
    int chunk_id_size = info_start - chunk_id; //Christ, just don't ask.
    int data_size = (&header[length] - 1) - data - 2; //First number is a pointer to the
                                                    //last character recieved (which would
                                                    //be '}', hence a -1). Second is
                                                    //the index of the colon before 'data'
                                                    //begins, (hence another -1). Pulled
                                                    //the third -1 out of my ass.

    char * chunk_id_s = new char[chunk_id_size + 1];
    chunk_id_s[chunk_id_size] = '\0';
    strncpy_s(chunk_id_s, chunk_id_size + 1, chunk_id, chunk_id_size);

    char * super_md5_s = new char[super_md5_size + 1];
    super_md5_s[super_md5_size] = '\0';
    strncpy_s(super_md5_s, super_md5_size + 1, super_md5 + 1, super_md5_size);

    char * md5_s = new char[md5_size + 1];
    md5_s[md5_size] = '\0';
    strncpy_s(md5_s, md5_size + 1, md5 + 1, md5_size); //'md5 + 1' to avoid the ':' character
    
    char * data_s = new char[data_size + 1]; //No need for null termination
    memcpy(data_s, data + 1, data_size); //'data + 1' to avoid the ':' character
                                         //also 'memcpy' to account for null bytes
                                         
    //Build resulting fileparts pointer and return
    FILEPARTS * f = new FILEPARTS;//new FILEPARTS;
    f->chunk_id = atoi(chunk_id);
    f->full_size = length;
    f->data_size = data_size;
    f->data = data_s;
    f->full_header = header;
    f->md5 = md5_s;
    f->super_md5 = super_md5_s;

    if (!statlist[f->super_md5]) {
        printf("Uh-oh. Looks like we're segfaulting.\n");
        printf("BMD5: \"%s\"\n", f->super_md5.c_str());
        //printf("BPOINTER: %d\n", statlist[f->super_md5]);

        printf("...BUT WAIT! Your valiant Knight, \"Shitty code\", is\nhere to save the day!\n");
        
        while (!statlist[f->super_md5]) {
            Sleep(10);
        }
    }

    f->stats = statlist[super_md5_s];
    statlist[f->super_md5]->parts[f->chunk_id] = f;// = f; //Set the proper index in the super array to equal us
    return f;
}

//Takes a file name, desired chunk number and reference to an int
//Returns the raw bytes for that specific chunk, and the size of
//the data.
unsigned char * get_chunk_data(std::string file, int chunk_number, int &data_size) {
    std::ifstream input_file(file.c_str(), std::ifstream::binary);
    int skip_to = chunk_number * CHUNK_SIZE;
    int total_size = get_file_size(file);

    data_size = CHUNK_SIZE;
    if ((total_size - skip_to) < CHUNK_SIZE) {
        data_size = total_size - skip_to;
    }

    unsigned char * buffer = new unsigned char[data_size];
    
    input_file.seekg((std::streampos)skip_to);
    input_file.read((char *)buffer, data_size);
    input_file.close();
    return buffer;
}

int calculate_chunk_number(std::string file) {
    int file_size = get_file_size(file);
    int chunk_number = file_size / CHUNK_SIZE;
    if ((file_size % CHUNK_SIZE) != 0) chunk_number++; //Add one for the last bit of data
    return chunk_number;
}

int get_file_size(std::string file) {
    std::streampos fsize = 0;
    std::ifstream input_file(file.c_str(), std::ifstream::binary);

    fsize = input_file.tellg();
    input_file.seekg( 0, std::ios::end );
    fsize = input_file.tellg() - fsize;
    input_file.close();

    return (int)fsize;
}

//Create file
void init_file(std::string file) {
    std::ofstream out(file.c_str(), std::ios_base::binary);
    out.close();
}

int write_chunk_to_file(std::string file, char * buf, int length, int chunk_number, int chunk_size) {
    //Create file if it doesn't exist
    if (!file_exists(file)) init_file(file);

    std::fstream out(file.c_str(), std::ios_base::binary | std::ios_base::out | std::ios_base::in);
    int pos = chunk_number * chunk_size;

    out.seekp(pos, std::ios_base::beg);
    out.write(buf, length);

    out.close();
    return 0;
}

int append_to_file(std::string file, char * buf, int length) {
    std::ofstream out(file.c_str(), std::ofstream::binary | std::ofstream::app);
    out.seekp(std::ios::end);
    out.write(buf, length);
    out.close();
    return 0;
}

std::string md5_file(std::string file) {
    std::string accumulated = "";
    unsigned char buffer[CHUNK_SIZE];
    std::ifstream input_file(file.c_str(), std::ifstream::binary);
    while (input_file.read((char *)buffer, CHUNK_SIZE))
    {
        accumulated.append(md5(buffer, (int)input_file.gcount()));
        memset(buffer, 0, CHUNK_SIZE);
    }
    input_file.close();

    //printf("ACC: %s\n", accumulated.c_str());

    return md5((unsigned char*)accumulated.c_str(), accumulated.length());
}

#ifdef _WIN32
std::string md5(unsigned char * buffer, int length) {
	DWORD dwStatus = 0;
	HCRYPTPROV hProv = 0;
	HCRYPTHASH hHash = 0;
	DWORD cbRead = 0;
	BYTE rgbHash[MD5LEN];
	DWORD cbHash = 0;
	CHAR rgbDigits[] = "0123456789abcdef";

	std::string ret; //return string

	// Get handle to the crypto provider
	if (!CryptAcquireContext(&hProv,
		NULL,
		NULL,
		PROV_RSA_FULL,
		CRYPT_VERIFYCONTEXT))
	{
		dwStatus = GetLastError();
		printf("CryptAcquireContext failed: %d\n", dwStatus);
	}

	if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
	{
		dwStatus = GetLastError();
		printf("CryptAcquireContext failed: %d\n", dwStatus);
		CryptReleaseContext(hProv, 0);
		return NULL;
	}

	if (!CryptHashData(hHash, buffer, length, 0))
	{
		dwStatus = GetLastError();
		printf("CryptHashData failed: %d\n", dwStatus);
		CryptReleaseContext(hProv, 0);
		CryptDestroyHash(hHash);
		return NULL;
	}

	cbHash = MD5LEN;
	if (CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0))
	{
		for (DWORD i = 0; i < cbHash; i++)
		{
			ret += rgbDigits[rgbHash[i] >> 4];
			ret += rgbDigits[rgbHash[i] & 0xf];
		}
	}
	else
	{
		dwStatus = GetLastError();
		printf("CryptGetHashParam failed: %d\n", dwStatus);
	}

	CryptDestroyHash(hHash);
	CryptReleaseContext(hProv, 0);
	return ret;
}
#endif

#ifdef __linux__
std::string md5(unsigned char * buffer, int length) {
    unsigned char * result = new unsigned char[MD5_DIGEST_LENGTH];

    MD5(buffer, length, result);

    std::ostringstream sout;
    sout<<std::hex<<std::setfill('0');
    for (int i = 0; i < sizeof(result); i++)
    {
        sout<<std::setw(2)<<(long long)result[i];
    }
    
    return sout.str();
}
#endif

//Ensures the connection has both tcp and udp counterparts
bool check_connection(MoonNetworking::Connection * c) {
    return (c->hasTCP() && c->hasUDP());
}

/* //TODO: Link this to a new file handling library
//Lists the files in the current directory
std::string list_files() {
    std::string ret;
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(".")) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            //printf ("%s\n", ent->d_name);
            ret.append(ent->d_name);
            ret.append("\n");
        }
        closedir(dir);
    } 
    else {
        return "";
    }
    return ret;
}
*/