// $Id: cixd.cpp,v 1.7 2016-05-09 16:01:56-07 - - $

#include <iostream>
#include <fstream> 
#include <memory>
#include <string>
#include <vector>
using namespace std;

#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include "protocol.h"
#include "logstream.h"
#include "sockets.h"

logstream log (cout);
struct cix_exit: public exception {};

void reply_get (accepted_socket& client_sock, cix_header& header) {
   ifstream send_stream (header.filename, ios::in | ios::binary);
   if(send_stream.fail()){
      cerr << "server: get: " << header.filename << 
      " "<< strerror (errno) << endl;
      memset (header.filename, 0, FILENAME_SIZE);
      header.command = cix_command::NAK;
      header.nbytes = errno;
      send_packet (client_sock, &header, sizeof header);
      return;
   }

   struct stat sb; 
   if(stat(header.filename, &sb) != 0){
      cerr << header.filename << strerror (errno) << endl;
      return;
   }

   auto buffer = make_unique<char[]> (sb.st_size);
   send_stream.read(buffer.get(), sb.st_size);
   header.command = cix_command::FILEOUT;
   header.nbytes = sb.st_size;
   send_packet (client_sock, &header, sizeof header);
   send_packet (client_sock, buffer.get(), header.nbytes);
}

void reply_ls (accepted_socket& client_sock, cix_header& header) {
   const char* ls_cmd = "ls -l 2>&1";
   FILE* ls_pipe = popen (ls_cmd, "r");
   if (ls_pipe == NULL) { 
      cerr << "ls -l: popen failed: " << strerror (errno) << endl;
      header.command = cix_command::NAK;
      header.nbytes = errno;
      send_packet (client_sock, &header, sizeof header);
      return;
   }
   string ls_output;
   char buffer[0x1000];
   for (;;) {
      char* rc = fgets (buffer, sizeof buffer, ls_pipe);
      if (rc == nullptr) break;
      ls_output.append (buffer);
   }
   int status = pclose (ls_pipe);
   if (status < 0) cerr << ls_cmd << ": " << strerror (errno) << endl;
              else cerr << ls_cmd << ": exit " << (status >> 8)
                       << " signal " << (status & 0x7F)
                       << " core " << (status >> 7 & 1) << endl;
   header.command = cix_command::LSOUT;
   header.nbytes = ls_output.size();
   memset (header.filename, 0, FILENAME_SIZE);
   send_packet (client_sock, &header, sizeof header);
   send_packet (client_sock, ls_output.c_str(), ls_output.size());
}

void reply_put (accepted_socket& client_sock, cix_header& header) {
   string str(header.filename);
   ofstream write_file( str , ios::out | ios::binary | ios::trunc);
   if(write_file.fail()){
      cerr << "put: open failed: " << strerror (errno) << endl;
      header.command = cix_command::NAK;
      header.nbytes = errno;
      memset (header.filename, 0, FILENAME_SIZE);
      send_packet (client_sock, &header, sizeof header);
      return;
   }
   auto buffer = make_unique<char[]> (header.nbytes);
   recv_packet (client_sock, buffer.get(), header.nbytes);
   write_file.write(buffer.get(), header.nbytes);
   if(write_file.bad()){
      cerr << "put: write failed: " << strerror (errno) << endl;
      header.command = cix_command::NAK;
      header.nbytes = errno;
      memset (header.filename, 0, FILENAME_SIZE);
      send_packet (client_sock, &header, sizeof header);
      return;
   }
   
   header.command = cix_command::ACK;
   memset (header.filename, 0, FILENAME_SIZE);
   send_packet (client_sock, &header, sizeof header);   
}


void reply_rm (accepted_socket& client_sock, cix_header& header) {
   int rc = unlink (header.filename);
   if ( rc == 0 ){
      header.command = cix_command::ACK;
      header.nbytes = 0;
   }else{

      cerr << "rm: " << header.filename <<
      " " << strerror (errno) << endl;
      header.command = cix_command::NAK;
      header.nbytes = errno;
   }
   memset (header.filename, 0, FILENAME_SIZE);
   send_packet (client_sock, &header, sizeof header);
   return;
}


void run_server (accepted_socket& client_sock) {
   log.execname (log.execname() + "-server");
   log << "connected to " << to_string (client_sock) << endl;
   try {   
      for (;;) {
         cix_header header; 
         recv_packet (client_sock, &header, sizeof header);
         switch (header.command) {
            case cix_command::GET: 
               reply_get (client_sock, header);
               break;
            case cix_command::LS: 
               reply_ls (client_sock, header);
               break;
            case cix_command::PUT: 
               reply_put (client_sock, header);
               break;
            case cix_command::RM: 
               reply_rm (client_sock, header);
               break;
            default:
               log << "invalid header from client:" << header << endl;
               break;
         }
      }
   }catch (socket_error& error) {
      cerr << error.what() << endl;
   }catch (cix_exit& error) {
      cerr << "caught cix_exit" << endl;
   }
   throw cix_exit();
}

void fork_cixserver (server_socket& server, accepted_socket& accept) {
   pid_t pid = fork();
   if (pid == 0) { // child
      server.close();
      run_server (accept);
      throw cix_exit();
   }else {
      accept.close();
      if (pid < 0) {
         cerr << "fork failed: " << strerror (errno) << endl;
      }
   }
}

void reap_zombies() {
   for (;;) {
      int status;
      pid_t child = waitpid (-1, &status, WNOHANG);
      if (child <= 0) break;
   }
}

void signal_handler (int) {
   reap_zombies();
}

void signal_action (int signal, void (*handler) (int)) {
   struct sigaction action;
   action.sa_handler = handler;
   sigfillset (&action.sa_mask);
   action.sa_flags = 0;
   int rc = sigaction (signal, &action, nullptr);
   if (rc < 0) cerr << "sigaction " << strsignal (signal) << " failed: "
                   << strerror (errno) << endl;
}

int main (int argc, char** argv) {
   log.execname (basename (argv[0]));
   vector<string> args (&argv[1], &argv[argc]);
   signal_action (SIGCHLD, signal_handler);
   in_port_t port = get_cix_server_port (args, 0);
   try {
      server_socket listener (port);
      for (;;) {
         log << to_string (hostinfo()) << " accepting port "
             << to_string (port) << endl;
         accepted_socket client_sock;
         for (;;) {
            try {
               listener.accept (client_sock);
               break;
            }catch (socket_sys_error& error) {
               switch (error.sys_errno) {
                  case EINTR:
                     cerr << "listener.accept caught "
                         << strerror (EINTR) << endl;
                     break;
                  default:
                     throw;
               }
            }
         }
         log << "accepted " << to_string (client_sock) << endl;
         try {
            fork_cixserver (listener, client_sock);
            reap_zombies();
         }catch (socket_error& error) {
            cerr << error.what() << endl;
         }
      }
   }catch (socket_error& error) {
      cerr << error.what() << endl;
   }catch (cix_exit& error) {
      cerr << "caught cix_exit" << endl;
   }
   return 0;
}

