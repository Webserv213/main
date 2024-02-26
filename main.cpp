#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <iostream>
#include <map>
#include <vector>

using namespace std;

void exit_with_perror(const string& msg)
{
    cerr << msg << endl;
    exit(EXIT_FAILURE);
}

void change_events(vector<struct kevent>& change_list, uintptr_t ident, int16_t filter,
        uint16_t flags, uint32_t fflags, intptr_t data, void *udata)
{
    struct kevent temp_event;

    EV_SET(&temp_event, ident, filter, flags, fflags, data, udata);
    change_list.push_back(temp_event);
}

void disconnect_client(int client_fd, map<int, string>& clients)
{
    cout << "client disconnected: " << client_fd << endl;
    close(client_fd);
    clients.erase(client_fd);
}

int main()
{
    vector<int> server_sockets;
    map<int, string> clients;
    vector<struct kevent> change_list;
    struct kevent event_list[8];

    for(int i = 0; i < 2; i++)
    {
        int server_socket;
        struct sockaddr_in server_addr;

        if ((server_socket = socket(PF_INET, SOCK_STREAM, 0)) == -1)
            exit_with_perror("socket() error\n" + string(strerror(errno)));

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (i == 0)
            server_addr.sin_port = htons(84);   
        else
            server_addr.sin_port = htons(85);   
        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
            exit_with_perror("bind() error\n" + string(strerror(errno)));

        if (listen(server_socket, 5) == -1)
            exit_with_perror("listen() error\n" + string(strerror(errno)));
        fcntl(server_socket, F_SETFL, O_NONBLOCK);
        server_sockets.push_back(server_socket);

        change_events(change_list, server_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
        cout << "echo server started" << endl;
    }
    
    /* init kqueue */
    int kq;
    if ((kq = kqueue()) == -1)
        exit_with_perror("kqueue() error\n" + string(strerror(errno)));


    /* main loop */
    int new_events;
    struct kevent* curr_event;
    while (1)
    {
        new_events = kevent(kq, &change_list[0], change_list.size(), event_list, 8, NULL);
        if (new_events == -1)
            exit_with_perror("kevent() error\n" + string(strerror(errno)));

        change_list.clear();

        for (int i = 0; i < new_events; ++i)
        {
            curr_event = &event_list[i];

            /* check error event return */
            if (curr_event->flags & EV_ERROR)
            {
                // if (curr_event->ident == server_socket)
                //     exit_with_perror("server socket error");
                // else
                // {
                //     cerr << "client socket error" << endl;
                //     disconnect_client(curr_event->ident, clients);
                // }
            }
            else if (curr_event->filter == EVFILT_READ)
            {
                int flag = false;
                for(int i = 0; i < server_sockets.size(); i++)
                {
                    if (curr_event->ident == server_sockets[i])
                    {
                        flag = true;
                        /* accept new client */
                        int client_socket;
                        if ((client_socket = accept(server_sockets[i], NULL, NULL)) == -1)
                            exit_with_perror("accept() error\n" + string(strerror(errno)));
                        cout << "accept new client: " << client_socket << endl;
                        fcntl(client_socket, F_SETFL, O_NONBLOCK);

                        /* add event for client socket - add read && write event */
                        change_events(change_list, client_socket, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
                        change_events(change_list, client_socket, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
                        clients[client_socket] = "";
                    }
                }
                if (flag == true)
                    continue;
                if (clients.find(curr_event->ident)!= clients.end())
                {
                    /* read data from client */
                    char buf[1024];
                    int n = read(curr_event->ident, buf, sizeof(buf));

                    if (n <= 0)
                    {
                        if (n < 0)
                            cerr << "client read error!" << endl;
                        disconnect_client(curr_event->ident, clients);
                    }
                    else
                    {
                        buf[n] = '\0';
                        clients[curr_event->ident] += buf;
                        cout << "received data from " << curr_event->ident << ": " << clients[curr_event->ident] << endl;
                    }
                }
            }
            else if (curr_event->filter == EVFILT_WRITE)
            {
                /* send data to client */
                map<int, string>::iterator it = clients.find(curr_event->ident);
                if (it != clients.end())
                {
                    if (clients[curr_event->ident] != "")
                    {
                        int n;
                        if ((n = write(curr_event->ident, clients[curr_event->ident].c_str(),
                                        clients[curr_event->ident].size()) == -1))
                        {
                            cerr << "client write error!" << endl;
                            disconnect_client(curr_event->ident, clients);  
                        }
                        else
                            clients[curr_event->ident].clear();
                    }
                }
            }
        }
    }
    return (0);
}
