#pragma once



#include "natives.h"





extern logprintf_t logprintf;

extern amxPool *gPool;
extern amxSocket *gSocket;
extern amxString *gString;

extern boost::mutex gMutex;

extern std::queue<amxConnectError> amxConnectErrorQueue;





const AMX_NATIVE_INFO amxNatives::addonNatives[] =
{
	{"InitAddon", amxNatives::Init},
	{"IsClientConnected", amxNatives::IsClientConnected},
	{"KickClient", amxNatives::KickClient},
	{"GetClientSerial", amxNatives::GetClientSerial},
	{"GetClientScreenshot", amxNatives::GetClientScreenshot},
	{"TransferLocalFile", amxNatives::TransferLocalFile},
	{"TransferRemoteFile", amxNatives::TransferRemoteFile},

	{NULL, NULL}
};



// native InitAddon(ip[], port, maxplayers);
cell AMX_NATIVE_CALL amxNatives::Init(AMX *amx, cell *params)
{
	if(!arguments(3))
	{
		logprintf("samp-addon: Invalid argument count in native 'samp_addon_init' (%i)", (params[0] >> 4));

		return NULL;
	}

	std::string ip = gString->Get(amx, params[1]);
	int port = params[2];
	int maxplayers = params[3];

	if(!ip.length() || (ip.length() > 15))
	{
		logprintf("samp-addon: NULL ip argument passed to native 'samp_addon_init'");

		return NULL;
	}

	if((port < 1024) || (port > 65535))
	{
		logprintf("samp-addon: Invalid port value passed to native 'samp_addon_init' (%i)", port);

		return NULL;
	}

	if((maxplayers < 0) || (maxplayers > 1000))
	{
		logprintf("samp-addon: Invalid max players value passed to native 'samp_addon_init' (%i)", maxplayers);

		return NULL;
	}

	if(gPool->pluginInit)
	{
		logprintf("samp-addon: Duplicate init interrupted");

		return NULL;
	}

	port++;

	logprintf("\nsamp-addon: Init with address: %s:%i, max clients is %i\n", ip.c_str(), port, maxplayers);

	boost::mutex::scoped_lock lock(gMutex);
	gPool->pluginInit = true;
	lock.unlock();

	gSocket = new amxSocket(ip, port, maxplayers);

	boost::thread process(boost::bind(&amxProcess::Thread));
	
	return 1;
}



// native IsClientConnected(clientid)
cell AMX_NATIVE_CALL amxNatives::IsClientConnected(AMX *amx, cell *params)
{
	if(!arguments(1))
	{
		logprintf("samp-addon: Invalid argument count in native 'IsClientConnected' (%i)", (params[0] >> 2));

		return NULL;
	}

	int clientid = params[1];

	return (gSocket->IsClientConnected(clientid)) ? 1 : 0;
}



// native KickClient(clientid)
cell AMX_NATIVE_CALL amxNatives::KickClient(AMX *amx, cell *params)
{
	if(!arguments(1))
	{
		logprintf("samp-addon: Invalid argument count in native 'KickClient' (%i)", (params[0] >> 2));

		return NULL;
	}

	int clientid = params[1];

	gSocket->KickClient(clientid);

	return 1;
}



// native GetClientSerial(clientid);
cell AMX_NATIVE_CALL amxNatives::GetClientSerial(AMX *amx, cell *params)
{
	if(!arguments(1))
	{
		logprintf("samp-addon: Invalid argument count in native 'GetClientVolumeID' (%i)", (params[0] >> 2));

		return NULL;
	}

	int clientid = params[1];

	if(!gSocket->IsClientConnected(clientid))
		return NULL;

	int ret = gPool->clientPool.find(clientid)->second.Client.Serial;

	return ret;
}



// native GetClientScreenshot(clientid, remote_filename[]);
cell AMX_NATIVE_CALL amxNatives::GetClientScreenshot(AMX *amx, cell *params)
{
	if(!arguments(2))
	{
		logprintf("samp-addon: Invalid argument count in native 'TakePlayerScreenshot' (%i)", (params[0] >> 2));

		return NULL;
	}

	int clientid = params[1];

	if(!gSocket->IsClientConnected(clientid))
		return NULL;

	std::string filename = gString->Get(amx, params[2]);

	if(!filename.length() || (filename.length() > 256) || (filename.find(".png") != (filename.length() - 4)))
	{
		logprintf("samp-addon: Invalid file name format");

		return NULL;
	}

	gSocket->Send(clientid, formatString() << "TCPQUERY SERVER_CALL" << " " << 1003 << " " << filename);

	return 1;
}



// native TransferLocalFile(file[], toclient, remote_filename[]);
cell AMX_NATIVE_CALL amxNatives::TransferLocalFile(AMX *amx, cell *params)
{
	if(!arguments(3))
		return NULL;

	int clientid = params[2];

	if(!gSocket->IsClientConnected(clientid))
		return NULL;

	cliPool cPool;

	boost::mutex::scoped_lock lock(gMutex);
	cPool = gPool->clientPool.find(clientid)->second;

	cPool.Transfer.Active = true;

	gPool->clientPool[clientid] = cPool;
	lock.unlock();

	boost::thread send(boost::bind(&amxTransfer::SendThread, clientid, gString->Get(amx, params[1]), gString->Get(amx, params[3])));

	return 1;
}



// native TransferRemoteFile(remote_filename[], fromclient, file[]);
cell AMX_NATIVE_CALL amxNatives::TransferRemoteFile(AMX *amx, cell *params)
{
	if(!arguments(3))
		return NULL;

	int clientid = params[2];

	if(!gSocket->IsClientConnected(clientid))
		return NULL;

	cliPool cPool;

	boost::mutex::scoped_lock lock(gMutex);
	cPool = gPool->clientPool.find(clientid)->second;

	cPool.Transfer.file = gString->Get(amx, params[3]);

	gPool->clientPool[clientid] = cPool;
	lock.unlock();

	gSocket->Send(clientid, formatString() << "TCPQUERY SERVER_CALL" << " " << 2001 << " " << gString->Get(amx, params[1]));

	return 1;
}