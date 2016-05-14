#ifdef UA_NO_AMALGAMATION
# include <time.h>
# include "ua_types.h"
# include "ua_server.h"
# include "ua_config_standard.h"
# include "networklayer_tcp.h"
#else
# include "open62541.h"
#endif

#include <signal.h>
#include <errno.h> // errno, EINTR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _MSC_VER
# include <io.h> //access
#else
# include <unistd.h> //access
#endif

#ifdef UA_ENABLE_MULTITHREADING
# ifdef UA_NO_AMALGAMATION
#  ifndef __USE_XOPEN2K
#   define __USE_XOPEN2K
#  endif
# endif
#include <pthread.h>
#endif

UA_Boolean running = 1;
UA_Logger logger = Logger_Stdout;

//mal bisl was einfügen, wa? ZeitStempel:

static UA_StatusCode readTimeData(void *handle, const UA_NodeId nodeId, UA_Boolean sourceTimeStamp, const UA_NumericRange *range, UA_DataValue *value) {
	if(range) {
		value->hasStatus = true;
		value->status = UA_STATUSCODE_BADINDEXRANGEINVALID;
		return UA_STATUSCODE_GOOD;
	}
	UA_DateTime currentTime = UA_DateTime_now();
	UA_Variant_setScalarCopy(&value->value, &currentTime, &UA_TYPES[UA_TYPES_DATETIME]);
	value->hasValue = true;
	if(sourceTimeStamp) {
		value->hasSourceTimestamp = true;
		value->sourceTimestamp = currentTime;
	}
	return UA_STATUSCODE_GOOD;
}

static void stopHandler(int sign) {
	UA_LOG_INFO(logger, UA_LOGCATEGORY_SERVER, "Received Ctrl-C");
	running = 0;
}

static UA_ByteString loadCertificate(void){
	UA_ByteString certificate = UA_STRING_NULL;
	FILE *fp = NULL;
	if(!(fp=fopen("server_cert.der", "rb"))){
		errno = 0;
		return certificate;
	}

	fseek(fp, 0, SEEK_END);
	certificate.length = (size_t)ftell(fp);
	certificate.data = malloc(certificate.length*sizeof(UA_Byte));
	if(!certificate.data){
		fclose(fp);
		return certificate;
	}

	fseek(fp, 0, SEEK_SET);
	if(fread(certificate.data, sizeof(UA_Byte), certificate.length, fp) < (size_t)certificate.length)
		UA_ByteString_deleteMembers(&certificate);
	fclose(fp);

	return certificate;
}

int main(int argc, char** argv) {
	signal(SIGINT, stopHandler); //catches ctrl-c

	UA_ServerNetworkLayer nl = UA_ServerNetworkLayerTCP(UA_ConnectionConfig_standard, 16664);
	UA_ServerConfig config = UA_ServerConfig_standard;
	config.serverCertificate = loadCertificate();
	config.networkLayers = &nl;
	config.networkLayersSize = 1;
	UA_Server *server = UA_Server_new(config);

	//so... jetzt wollen wir mal unseren node von oben hinzufügen
	UA_DataSource dateDataSource = (UA_DataSource) {.handle = NULL, .read = readTimeData, .write = NULL};
	UA_VariableAttributes v_attr;
	UA_VariableAttributes_init(&v_attr);
	v_attr.description = UA_LOCALIZEDTEXT("en_US","current Time");
	v_attr.displayName = UA_LOCALIZEDTEXT("en_US","current Time");
	v_attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
	const UA_QualifiedName dateName = UA_QUALIFIEDNAME(1, "current Time");
	UA_NodeId dataSourceId;
	UA_Server_addDataSourceVariableNode(server, UA_NODEID_NULL,
										UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
										UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), dateName,
										UA_NODEID_NULL, v_attr, dateDataSource, &dataSourceId);

//Server starten
UA_StatusCode retval = UA_Server_run(server, &running);

UA_Server_delete(server);
nl.deleteMembers(&nl);

UA_ByteString_deleteMembers(&config.serverCertificate);
return (int)retval;
}
