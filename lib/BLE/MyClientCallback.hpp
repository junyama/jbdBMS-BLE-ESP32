#ifndef MY_CLIENT_CALLBACK_HPP
#define MY_CLIENT_CALLBACK_HPP

#include "MyDataProcess.hpp"

class MyClientCallback : public BLEClientCallbacks
{ // this is called on connect / disconnect by some underlying magic+
private:
	static const String TAG;
	void onConnect(BLEClient *pclient);
	void onDisconnect(BLEClient *pclient);

public:
	bool BLE_client_connected;
	MyClientCallback();
	MyClientCallback(boolean *p_BLE_client_connected);
};

#endif /* MY_CLIENT_CALLBACK_HPP */