#ifndef MY_CLIENT_CALLBACK_HPP
#define MY_CLIENT_CALLBACK_HPP

#include "MyCommon.hpp"

class MyClientCallback : public BLEClientCallbacks
{ // this is called on connect / disconnect by some underlying magic+
private:
	const String TAG;
	void onConnect(BLEClient *pclient);
	void onDisconnect(BLEClient *pclient);

public:
	MyClientCallback();
};

#endif /* MY_CLIENT_CALLBACK_HPP */