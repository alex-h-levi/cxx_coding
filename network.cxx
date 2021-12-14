#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <iomanip>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fstream>
#include <thread>
#include <string>
#include <iterator>
#include <regex>
#include <errno.h>

using namespace std;

constexpr socklen_t client_buffer_len = 256;
constexpr int max_srv_connections = 5;

void log_error(string pprefix, string perrmsg) {
    cerr << pprefix << ": " << perrmsg << endl;
}

/**
 * @brief Server thread to receive messages from the client
 * 
 * @param port port number to bind to
 */
void server_thread(int port) {
    int sockfd, cli_sockfd, portno;
    socklen_t cli_addr_len;
    char client_buffer[client_buffer_len];
    struct sockaddr_in serv_addr, cli_addr;
    int bytes_read;
    ostringstream m;
    bool client_on = true;
    sockfd =  socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        log_error("SOCKET ERROR",  "cannot create stream socket");
        memset(&serv_addr, 0, sizeof(serv_addr));
        portno = port;
        serv_addr.sin_family = AF_INET;  
        serv_addr.sin_addr.s_addr = INADDR_ANY;  
        serv_addr.sin_port = htons(portno);

        if (
            bind(
                sockfd, 
                (struct sockaddr *) &serv_addr,
                sizeof(serv_addr)
            ) < 0
        ) 
              log_error("SERVER ERROR",  "on binding to port");

        listen(sockfd, max_srv_connections);
        cli_addr_len = sizeof(cli_addr);
        cli_sockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &cli_addr_len);
        if (cli_sockfd < 0) 
            log_error("ERROR", "on accept a new client connection");

        std::cout << "[SERVER]: connection from " << inet_ntoa(cli_addr.sin_addr) << ":" << ntohs(cli_addr.sin_port);

        send(cli_sockfd, "[HANDSHAKE]:WELCOME\n", 13, 0);

        regex rx_buy(R"((BYEBYE)|(buybuy))");
        while(client_on){
                memset(client_buffer, 0, client_buffer_len);

                bytes_read = read(cli_sockfd, client_buffer, client_buffer_len-1);
                if (bytes_read < 0) log_error("ERROR", "reading from socket");
                std::cout << "[SERVER] Message from client:" << client_buffer;
                
                if(regex_search(string(client_buffer), rx_buy)) {
                    client_on = false;
                }
        }
        std::cout << "[SERVER]: server thread terminated." << endl;
        close(cli_sockfd);
        close(sockfd);
}

void client_thread(const string& data_file_path, int port) {
    int sockfd, portno, bytes_sent;
    struct sockaddr_in serv_addr;
    struct hostent *server_addr;

    char client_buffer[client_buffer_len];
    portno = port;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        log_error("ERROR", "opening socket");
    char my_host_name[_SC_HOST_NAME_MAX+1];
    memset(my_host_name, 0, sizeof(my_host_name));
    gethostname(my_host_name, sizeof(my_host_name));
    server_addr = gethostbyname(my_host_name);
    if (server_addr == NULL) {
        log_error("[CLIENT ERROR}", "no such host");
        return;
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(server_addr->h_addr, 
         &serv_addr.sin_addr.s_addr,
         server_addr->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
        log_error("ERROR", "connecting");

    memset(client_buffer, 0, client_buffer_len);
    // read the file and send the contents over
    ifstream data_file;
    data_file.open(data_file_path);
    if (data_file.is_open() && data_file.good()) {
        istream_iterator<string> it_data_file(data_file);
        istream_iterator<string> it_data_file_end;
        ostringstream msg_buff;
        regex rx(R"(\n)");
        smatch s;
        while(it_data_file != it_data_file_end) {
            string text_line = *it_data_file++;
            msg_buff << text_line << " ";
            if (regex_search(text_line, rx)) {
                msg_buff << endl;
            }
        }
        msg_buff.str().c_str();
        bytes_sent = write(sockfd, msg_buff.str().c_str(), msg_buff.str().length());
    }
    if (bytes_sent < 0) 
         log_error("ERROR", "writing to socket");
    memset(client_buffer, 0, client_buffer_len);
    int bytes_rcvd = read(sockfd, client_buffer, 255);
    if (bytes_rcvd < 0) 
         log_error("ERROR", "reading from socket");
    cout << "[CLIENT] Server response: " << client_buffer << endl;
    cout.flush();
    close(sockfd);
    cout << "Client network thread terminates" << endl;
    cout.flush();
}

int main(int argc, char** argv, char** env) {
    thread srv_tx(server_thread, 35000);
    string data_file_path("./test.txt");
    thread clt_tx(client_thread, data_file_path, 35000);

    srv_tx.join();
    clt_tx.join();
}