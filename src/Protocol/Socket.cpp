// Source code from MC-Server, will be removed soon

#include "Globals.h"  // NOTE: MSVC stupidness requires this to be the same across all modules

#include "Socket.h"

#ifndef _WIN32
        #include <netdb.h>
        #include <unistd.h>
        #include <arpa/inet.h>                //inet_ntoa()
#else
        #define socklen_t int
#endif





cSocket::cSocket(xSocket a_Socket)
        : m_Socket(a_Socket)
{
}





cSocket::~cSocket()
{
        // Do NOT close the socket; this class is an API wrapper, not a RAII!
}





cSocket::operator cSocket::xSocket() const
{
        return m_Socket;
}





cSocket::xSocket cSocket::GetSocket() const
{
        return m_Socket;
}





bool cSocket::IsValidSocket(cSocket::xSocket a_Socket)
{
        #ifdef _WIN32
        return (a_Socket != INVALID_SOCKET);
        #else  // _WIN32
        return (a_Socket >= 0);
        #endif  // else _WIN32
}





void cSocket::CloseSocket()
{
        #ifdef _WIN32
        
        closesocket(m_Socket);
        
        #else  // _WIN32
        
        if (shutdown(m_Socket, SHUT_RDWR) != 0)//SD_BOTH);
        {
                LOGWARN("Error on shutting down socket %d (%s): %s", m_Socket, m_IPString.c_str(), GetLastErrorString().c_str());
        }
        if (close(m_Socket) != 0)
        {
                LOGWARN("Error closing socket %d (%s): %s", m_Socket, m_IPString.c_str(), GetLastErrorString().c_str());
        }
        
        #endif  // else _WIN32

        // Invalidate the socket so that this object can be re-used for another connection
        m_Socket = INVALID_SOCKET;
}





AString cSocket::GetErrorString( int a_ErrNo )
{
        char buffer[ 1024 ];
        AString Out;

        #ifdef _WIN32

        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, a_ErrNo, 0, buffer, ARRAYCOUNT(buffer), NULL);
        Printf(Out, "%d: %s", a_ErrNo, buffer);
        if (!Out.empty() && (Out[Out.length() - 1] == '\n'))
        {
                Out.erase(Out.length() - 2);
        }
        return Out;
        
        #else  // _WIN32
        
        // According to http://linux.die.net/man/3/strerror_r there are two versions of strerror_r():
        
        #if ( _GNU_SOURCE ) && !defined(ANDROID_NDK)  // GNU version of strerror_r()
        
        char * res = strerror_r( errno, buffer, ARRAYCOUNT(buffer) );
        if( res != NULL )
        {
                Printf(Out, "%d: %s", a_ErrNo, res);
                return Out;
        }
        
        #else  // XSI version of strerror_r():
        
        int res = strerror_r( errno, buffer, ARRAYCOUNT(buffer) );
        if( res == 0 )
        {
                Printf(Out, "%d: %s", a_ErrNo, buffer);
                return Out;
        }
        
        #endif  // strerror_r() version
        
        else
        {
                Printf(Out, "Error %d while getting error string for error #%d!", errno, a_ErrNo);
                return Out;
        }
        
        #endif  // else _WIN32
}




int cSocket::GetLastError()
{
#ifdef _WIN32
        return WSAGetLastError();
#else
        return errno;
#endif
}





bool cSocket::SetReuseAddress(void)
{
        #if defined(_WIN32) || defined(ANDROID_NDK)
                char yes = 1;
        #else
                int yes = 1;
        #endif
        return (setsockopt(m_Socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == 0);
}





int cSocket::WSAStartup(void)
{
#ifdef _WIN32
        WSADATA wsaData;
        memset(&wsaData, 0, sizeof(wsaData));
        return ::WSAStartup(MAKEWORD(2, 2),&wsaData);
#else
        return 0;
#endif
}





cSocket cSocket::CreateSocket(eFamily a_Family)
{
        return socket((int)a_Family, SOCK_STREAM, 0);
}





bool cSocket::BindToAnyIPv4(unsigned short a_Port)
{
        sockaddr_in local;
        memset(&local, 0, sizeof(local));

        local.sin_family = AF_INET;
        local.sin_port = htons((u_short)a_Port);

        return (bind(m_Socket, (sockaddr *)&local, sizeof(local)) == 0);
}





bool cSocket::BindToAnyIPv6(unsigned short a_Port)
{
        sockaddr_in6 local;
        memset(&local, 0, sizeof(local));

        local.sin6_family = AF_INET6;
        local.sin6_port = htons((u_short)a_Port);

        return (bind(m_Socket, (sockaddr *)&local, sizeof(local)) == 0);
}





bool cSocket::BindToLocalhostIPv4(unsigned short a_Port)
{
        sockaddr_in local;
        memset(&local, 0, sizeof(local));

        local.sin_family = AF_INET;;
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        local.sin_port = htons((u_short)a_Port);

        return (bind(m_Socket, (sockaddr*)&local, sizeof(local)) == 0);
}





bool cSocket::Listen(int a_Backlog)
{
        return (listen(m_Socket, a_Backlog) == 0);
}





cSocket cSocket::AcceptIPv4(void)
{
        sockaddr_in from;
        socklen_t fromlen = sizeof(from);

        cSocket SClient = accept(m_Socket, (sockaddr *)&from, &fromlen);

        if (SClient.IsValid() && (from.sin_addr.s_addr != 0))  // Get IP in string form
        {
                SClient.m_IPString = inet_ntoa(from.sin_addr);
        }
        return SClient;
}





cSocket cSocket::AcceptIPv6(void)
{
        sockaddr_in6 from;
        socklen_t fromlen = sizeof(from);

        cSocket SClient = accept(m_Socket, (sockaddr *)&from, &fromlen);

        // Get IP in string form:
        if (SClient.IsValid())
        {
                #if defined(_WIN32)
                        // Windows XP doesn't have inet_ntop, so we need to improvise. And MSVC has different headers than GCC
                        #ifdef _MSC_VER
                                // MSVC version
                                Printf(SClient.m_IPString, "%x:%x:%x:%x:%x:%x:%x:%x", 
                                        from.sin6_addr.u.Word[0],
                                        from.sin6_addr.u.Word[1],
                                        from.sin6_addr.u.Word[2],
                                        from.sin6_addr.u.Word[3],
                                        from.sin6_addr.u.Word[4],
                                        from.sin6_addr.u.Word[5],
                                        from.sin6_addr.u.Word[6],
                                        from.sin6_addr.u.Word[7]
                                );
                        #else  // _MSC_VER
                                // MinGW
                                Printf(SClient.m_IPString, "%x:%x:%x:%x:%x:%x:%x:%x", 
                                        from.sin6_addr.s6_addr16[0],
                                        from.sin6_addr.s6_addr16[1],
                                        from.sin6_addr.s6_addr16[2],
                                        from.sin6_addr.s6_addr16[3],
                                        from.sin6_addr.s6_addr16[4],
                                        from.sin6_addr.s6_addr16[5],
                                        from.sin6_addr.s6_addr16[6],
                                        from.sin6_addr.s6_addr16[7]
                                );
                        #endif  // else _MSC_VER
                #else
                        char buffer[INET6_ADDRSTRLEN];
                        inet_ntop(AF_INET6, &(from.sin6_addr), buffer, sizeof(buffer));
                        SClient.m_IPString.assign(buffer);
                #endif  // _WIN32
        }
        return SClient;
}





bool cSocket::ConnectToLocalhostIPv4(unsigned short a_Port)
{
        sockaddr_in server;
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        server.sin_port = htons(a_Port);
        return (connect(m_Socket, (sockaddr *)&server, sizeof(server)) == 0);
}





bool cSocket::ConnectIPv4(const AString & a_HostNameOrAddr, unsigned short a_Port)
{
        // First try IP Address string to hostent conversion, because it's faster
        unsigned long addr = inet_addr(a_HostNameOrAddr.c_str());
        if (addr == INADDR_NONE)
        {
                // It is not an IP Address string, but rather a regular hostname, resolve:
                hostent * hp = gethostbyname(a_HostNameOrAddr.c_str());
                if (hp == NULL)
                {
                        LOGWARNING("%s: Could not resolve hostname \"%s\"", __FUNCTION__, a_HostNameOrAddr.c_str());
                        CloseSocket();
                        return false;
                }
                addr = *((unsigned long*)hp->h_addr);
        }

        sockaddr_in server;
        server.sin_addr.s_addr = addr;
        server.sin_family = AF_INET;
        server.sin_port = htons((unsigned short)a_Port);
        return (connect(m_Socket, (sockaddr *)&server, sizeof(server)) == 0);
}





int cSocket::Receive(char* a_Buffer, unsigned int a_Length, unsigned int a_Flags)
{
        return recv(m_Socket, a_Buffer, a_Length, a_Flags);
}





int cSocket::Send(const char * a_Buffer, unsigned int a_Length)
{
        return send(m_Socket, a_Buffer, a_Length, 0);
}





unsigned short cSocket::GetPort(void) const
{
        ASSERT(IsValid());
        
        sockaddr_in Addr;
        socklen_t AddrSize = sizeof(Addr);
        if (getsockname(m_Socket, (sockaddr *)&Addr, &AddrSize) != 0)
        {
                return 0;
        }
        return ntohs(Addr.sin_port);
}
