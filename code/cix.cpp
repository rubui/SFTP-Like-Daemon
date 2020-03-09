// $Id: cix.cpp,v 1.7 2019-02-07 15:14:37-08 - - $

#include <iostream>
#include <fstream> 
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
using namespace std;

#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "protocol.h"
#include "logstream.h"
#include "sockets.h"

logstream log (cout);
struct cix_exit: public exception {};

unordered_map<string,cix_command> command_map {
   {"exit", cix_command::EXIT},  
   {"get" , cix_command::GET },
   {"help", cix_command::HELP},
   {"ls"  , cix_command::LS  },
   {"put" , cix_command::PUT },
   {"rm"  , cix_command::RM  },
};

static const string help = R"||(
exit         - Exit the program.  Equivalent to EOF.
get filename - Copy remote file to local host.
help         - Print help summary.
ls           - List names of files on remote server.
put filename - Copy local file to remote host.
rm filename  - Remove file from remote server.
)||";

void cix_get(client_socket& server, const string& filename){
   cix_header header;
   header.command = cix_command::GET;
   copy(filename.begin(), filename.end(), header.filename);
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   if (header.command != cix_command::FILEOUT) {
      cerr  << "sent GET, server returned NAK" << endl;
   }else {
      ofstream write_file( filename, ios::out | 
         ios::binary | ios::trunc);
      if(write_file.fail()){
         cerr << "get: " << filename << " open failed " 
         << strerror (errno) << endl;
         memset (header.filename, 0, FILENAME_SIZE);
         header.command = cix_command::ERROR;
         header.nbytes = errno;
         return;
      } 
      auto buffer = make_unique<char[]> (header.nbytes);
      recv_packet (server, buffer.get(), header.nbytes);
      write_file.write(buffer.get(), header.nbytes);
      memset (header.filename, 0, FILENAME_SIZE);
      if(write_file.bad()){
         cerr << "get: " << filename << " " << 
         strerror (errno) << endl;
         header.command = cix_command::ERROR;
         header.nbytes = errno;
         return;
      }  
   }
}

void cix_help() {
   cout << help;
}


void cix_ls (client_socket& server) {
   cix_header header;
   header.command = cix_command::LS;
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   if (header.command != cix_command::LSOUT) {
      cerr  << "sent LS, server did not return LSOUT" << endl;
   }else {
      auto buffer = make_unique<char[]> (header.nbytes + 1);
      recv_packet (server, buffer.get(), header.nbytes);
      buffer[header.nbytes] = '\0';
      cout << buffer.get();
   }
}

void cix_put(client_socket& server, const string& filename){
   cix_header header;
   header.command = cix_command::PUT;
   ifstream send_stream (filename, ifstream::binary);// | ios::in);
   if(send_stream.fail()){
      cerr << "put: " << filename << " " << strerror (errno) << endl;
      memset (header.filename, 0 , FILENAME_SIZE);
      header.command = cix_command::ERROR;
      header.nbytes = errno;
      return;
   }
   struct stat sb;
   const char* file_name = filename.c_str();
   if(stat(file_name, &sb) != 0){
      cerr << filename << strerror (errno) << endl;
      return;
   }
   auto buffer = make_unique<char[]> (sb.st_size);
   send_stream.read(buffer.get(), sb.st_size);
   header.nbytes = sb.st_size;
   copy(filename.begin(), filename.end(), header.filename);
   send_packet (server, &header, sizeof header);
   send_packet (server, buffer.get(), header.nbytes); 
   recv_packet (server, &header, sizeof header);
   if (header.command != cix_command::ACK) {
      cerr  << "sent PUT, server returned NAK" << endl;
   } else{
      cout  << "sent PUT, server returned ACK" << endl;
   }            
}

void cix_rm (client_socket& server, const string& filename) {
   cix_header header;
   header.command = cix_command::RM;
   copy(filename.begin(), filename.end(), header.filename);
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   if (header.command != cix_command::ACK) {
      cerr  << "sent RM, server returned NAK" << endl;
   }else{
      cout  << "sent RM, server returned ACK" << endl;
   }
}

void usage() {
   cerr << "Usage: " << log.execname() << " [host] [port]" << endl;
   throw cix_exit();
}

int main (int argc, char** argv) {
   log.execname (basename (argv[0]));
   vector<string> args (&argv[1], &argv[argc]);
   if (args.size() > 2) usage();
   string host = get_cix_server_host (args, 0);
   in_port_t port = get_cix_server_port (args, 1);
   log << to_string (hostinfo()) << endl;
   try {
      log << "connecting to " << host << " port " << port << endl;
      client_socket server (host, port);
      log << "connected to " << to_string (server) << endl;
      for (;;) {
         string line, tok;
         char delim = ' ';
         getline (cin, line);
         vector<string> cix_cmd_args;
         stringstream ss(line); 
         
         while(getline(ss, tok, delim)) {
           cix_cmd_args.push_back(tok);
         }

         if (cin.eof()) throw cix_exit();
         const auto& itor = command_map.find (cix_cmd_args[0]);
         cix_command cmd = itor == command_map.end()
                         ? cix_command::ERROR : itor->second;

         switch (cmd) {
            case cix_command::EXIT:
               throw cix_exit();
               break;
            case cix_command::GET:
               if (cix_cmd_args.size() == 2)
                  cix_get(server , cix_cmd_args[1]);
               else
                  log << "Usage: get filename" << endl;
               break;
            case cix_command::HELP:
               cix_help();
               break;
            case cix_command::LS:
               cix_ls (server);
               break;
            case cix_command::PUT:
               if (cix_cmd_args.size() == 2)
                  cix_put(server , cix_cmd_args[1]);
               else
                  log << "Usage: put filename" << endl;
               break;
            case cix_command::RM:
               if (cix_cmd_args.size() == 2)
                  cix_rm(server , cix_cmd_args[1]);
               else
                  log << "Usage: rm filename" << endl;
               break;
            default:
               log << line << ": invalid command" << endl;
               break;
         }
      }
   }catch (socket_error& error) {
      cerr << error.what() << endl;
   }catch (cix_exit& error) {
      cerr << "caught cix_exit" << endl;
   }
   return 0;
}

