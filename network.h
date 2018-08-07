#ifndef __NETWORK_H__
#define __NETWORK_H__
#include "message.h"

#include <bitcoin/protocol.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <vector>
#include <list>
#include <queue>
#include <map>
#include <functional>


struct event {
	void *ud;
	bool read;
	bool write;
	bool error;
};

#if defined(__linux__)
#include <sys/epoll.h>

static int sp_create(size_t) {
	return epoll_create(1024);
}

static void 
sp_del(int efd, int sock) {
	epoll_ctl(efd, EPOLL_CTL_DEL, sock , NULL);
}

static int sp_add(int efd, int sock, void *ud) {
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLOUT;
	ev.data.ptr = ud;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, sock, &ev) == -1) {
		return -1;
	}
	return 0;
}

static int sp_add_read(int efd, int sock, void *ud) {
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.ptr = ud;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, sock, &ev) == -1) {
		return -1;
	}
	return 0;
}

static int sp_add_write(int efd, int sock, void *ud) {
	struct epoll_event ev;
	ev.events = EPOLLOUT | EPOLLIN;
	ev.data.ptr = ud;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, sock, &ev) == -1) {
		return -1;
	}
	return 0;
}

static int sp_enable_write(int efd, int sock, void *ud) {
	struct epoll_event ev;
	ev.events = EPOLLOUT | EPOLLIN;
	ev.data.ptr = ud;
	if (epoll_ctl(efd, EPOLL_CTL_MOD, sock, &ev) == -1) {
		return -1;
	}
	return 0;
}

static int sp_disable_write(int efd, int sock, void *ud) {
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.ptr = ud;
	if (epoll_ctl(efd, EPOLL_CTL_MOD, sock, &ev) == -1) {
		return -1;
	}
	return 0;
}

static int 
sp_wait(int efd, struct event *e, int max,  const timespec *timeout) {
	struct epoll_event ev[max];
	int n = epoll_wait(efd , ev, max, -1);
	int i;
	for (i=0;i<n;i++) {
		e[i].ud = ev[i].data.ptr;
		unsigned flag = ev[i].events;
		e[i].write = (flag & EPOLLOUT) != 0;
		e[i].read = (flag & (EPOLLIN | EPOLLHUP)) != 0;
		e[i].error = (flag & EPOLLERR) != 0;
	}

	return n;
}

#elif defined(__APPLE__)
#include <sys/event.h>

inline int sp_create(size_t) {
    return kqueue();
}

static void sp_del(int kfd, int sock) {
	struct kevent ke;
	EV_SET(&ke, sock, EVFILT_READ, EV_DELETE, 0, 0, NULL);
	kevent(kfd, &ke, 1, NULL, 0, NULL);
	EV_SET(&ke, sock, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
	kevent(kfd, &ke, 1, NULL, 0, NULL);
}

static int sp_add_read(int kfd, int sock, void *ud) {
	struct kevent ke;
	EV_SET(&ke, sock, EVFILT_READ, EV_ADD, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1 ||	ke.flags & EV_ERROR) {
		return -1;
	}
	return 0;
}

static int sp_add_write(int kfd, int sock, void *ud) {
	struct kevent ke;
	EV_SET(&ke, sock, EVFILT_WRITE, EV_ADD, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1 ||	ke.flags & EV_ERROR) {
		return -1;
	}
	return 0;
}

static int sp_enable_write(int kfd, int sock, void *ud)
{
	struct kevent ke;
	EV_SET(&ke, sock, EVFILT_WRITE, EV_ENABLE, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1 ||	ke.flags & EV_ERROR) {
		sp_del(kfd, sock);
		return -1;
	}
	return 0;
}

static int sp_disable_write(int kfd, int sock, void *ud)
{
	struct kevent ke;
	EV_SET(&ke, sock, EVFILT_WRITE, EV_DISABLE, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1 ||	ke.flags & EV_ERROR) {
		sp_del(kfd, sock);
		return -1;
	}
	return 0;
}

static int 
sp_add(int kfd, int sock, void *ud) {
	struct kevent ke;
	EV_SET(&ke, sock, EVFILT_READ, EV_ADD, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1 ||	ke.flags & EV_ERROR) {
		return -1;
	}
	EV_SET(&ke, sock, EVFILT_WRITE, EV_ADD, 0, 0, ud);
	if (kevent(kfd, &ke, 1, NULL, 0, NULL) == -1 ||	ke.flags & EV_ERROR) {
		EV_SET(&ke, sock, EVFILT_READ, EV_DELETE, 0, 0, NULL);
		kevent(kfd, &ke, 1, NULL, 0, NULL);
		return -1;
	}
	return 0;
}


static int 
sp_wait(int kfd, struct event *e, int max, const timespec *timeout) {
	struct kevent ev[max];
	int n = kevent(kfd, NULL, 0, ev, max, timeout);

	int i;
	for (i=0;i<n;i++) {
		e[i].ud = ev[i].udata;
		unsigned filter = ev[i].filter;
		e[i].write = (filter == EVFILT_WRITE);
		e[i].read = (filter == EVFILT_READ);
		e[i].error = false;	// kevent has not error event
	}

	return n;
}
#else
#error only support linux and apple
#endif

enum ConnectionStatus {
	INIT,
	CONNECTING,
	CONNECTED,
	VERSION_SENT
};

class Connection
{
public:
	Connection(): sock(-1) {
		init();
	}
	Connection(int _sock, const CService &addr): sock(_sock), addrYou(addr) {
        init();
	}
	Connection(const Connection &con) = default;

	bool initializeAddress();
	bool pushVersionCommand(uint32_t);
	bool pushVerackCommand();
	bool pushPongCommand(uint64_t nonce);
	bool pushCommand();
	bool sendBuffer(bool &);
	void processMessage(uint32_t, struct CMessageHeader &header, const std::vector<unsigned char> &buffer, size_t offset);
	bool readBuffer(uint32_t version);
    void init() {
        status = INIT;
        youVersion = 0;
		youServices = 0;
		headerValid = false;
		sendPos = 0;
    }
	int sock;
	enum ConnectionStatus status;
    uint32_t youVersion;
    uint64_t youServices;
    CService addrMe;
	CService addrYou;
	bool headerValid;
	CMessageHeader header;
	int sendPos;
	std::vector<unsigned char> vReadBuffer;
	std::vector<unsigned char> vSendBuffer;
	std::list<std::vector<unsigned char>> sendingBuffer;
};

typedef std::function<bool (const struct event &event, int &sock, bool &moreWrite)> NetworkCallback;


class ConnectionManager
{
public:
	ConnectionManager(uint32_t version): mVersion(version) {}
	NetworkCallback *initiateConnection(const CService &addr, int &sock);
	bool networkCallback(int sock, const struct event &event, int &rsock, bool &moreWrite);
	int evictSock();
	NetworkCallback *getNetworkCallback(int sock) {
		return &callbacks[sock];
	}
	void closeConnection(int sock);
	size_t connectionCount() {
		return connections.size();
	}
private:
	uint32_t mVersion;
	Connection *addConnection(const CService &addr, int sock);
	std::map<int, Connection> connections;
	std::map<int, NetworkCallback> callbacks;
	std::deque<int> qSocks;
};

class NetworkEngine
{
public:
	NetworkEngine(uint32_t version): connMan(version), nPendingEvents(0), sp(-1) {}
    bool initEngine();
	void startEngine();
	void remove_socket(int sock) {
		sp_del(sp, sock);
	}
	int add_socket(int sock, NetworkCallback *cb) {
		return sp_add(sp, sock, (void *)cb);
	}
private:
	void dispatchNetworkEvents(int nActiveEvents);
	int sp;
	ConnectionManager connMan;
	int maxConnections;
	std::vector<event> events;
	std::map<int, bool> writeEnabled;
	int nPendingEvents;
};
#endif