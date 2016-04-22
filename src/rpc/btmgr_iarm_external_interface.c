#include "btmgr_priv.h"
#include "btmgr_iarm_interface.h"
#include <unistd.h>

#ifdef BTMGR_ENABLE_IARM_INTERFACE
static unsigned char isBTMGR_Inited = 0;
static BTMGR_EventCallback m_eventCallbackFunction = NULL;

const char* BTMGR_IARM_DEBUG_ACTUAL_PATH    = "/etc/debug.ini";
const char* BTMGR_IARM_DEBUG_OVERRIDE_PATH  = "/opt/debug.ini";

static void _btmgr_deviceCallback(const char *owner, IARM_EventId_t eventId, void *data, size_t len);

/*********************/
/* Public Interfaces */
/*********************/
BTMGR_Result_t BTMGR_Init()
{
    const char* pDebugConfig = NULL;

    if (!isBTMGR_Inited)
    {
        isBTMGR_Inited = 1;

        /* Init the logger */
        if( access(BTMGR_IARM_DEBUG_OVERRIDE_PATH, F_OK) != -1 )
            pDebugConfig = BTMGR_IARM_DEBUG_OVERRIDE_PATH;
        else
            pDebugConfig = BTMGR_IARM_DEBUG_ACTUAL_PATH;

        rdk_logger_init(pDebugConfig);
        
        IARM_Bus_Init("BTMgr-User");
        IARM_Bus_Connect();

        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_OUT_OF_RANGE, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_DISCOVERY_COMPLETE, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST, _btmgr_deviceCallback);
        BTMGRLOG_INFO ("IARM Interface Inited Successfully");
    }
    else
        BTMGRLOG_INFO ("IARM Interface Already Inited");

    return BTMGR_RESULT_SUCCESS;
}

BTMGR_Result_t BTMGR_DeInit()
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

    if (isBTMGR_Inited)
    {
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "DeInit", 0, 0);
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("BTMGR_DeInit: Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_DeInit: Failed; RetCode = %d", retCode);
        }
    }
    else
        BTMGRLOG_INFO ("IARM Interface for BTMgr is Not Inited Yet..");

    return rc;
}

BTMGR_Result_t BTMGR_GetNumberOfAdapters(unsigned char *pNumOfAdapters)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    unsigned char num_of_adapters = 0;
    
    if (!pNumOfAdapters)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "GetNumberOfAdapters", (void *)&num_of_adapters, sizeof(num_of_adapters));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            *pNumOfAdapters = num_of_adapters;
            BTMGRLOG_INFO ("BTMGR_GetNumberOfAdapters : Success; Number of Adapters = %d", num_of_adapters);
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_GetNumberOfAdapters : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_SetAdapterName(unsigned char index_of_adapter, const char* pNameOfAdapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterName_t adapterSetting;

    if((NULL == pNameOfAdapter) || (BTMGR_ADAPTER_COUNT_MAX < index_of_adapter))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        adapterSetting.m_adapterIndex = index_of_adapter;
        strncpy (adapterSetting.m_name, pNameOfAdapter, (BTMGR_NAME_LEN_MAX - 1));
        
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "SetAdapterName", (void *)&adapterSetting, sizeof(adapterSetting));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("BTMGR_SetAdapterName : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_SetAdapterName : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetAdapterName(unsigned char index_of_adapter, char* pNameOfAdapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterName_t adapterSetting;

    if((NULL == pNameOfAdapter) || (BTMGR_ADAPTER_COUNT_MAX < index_of_adapter))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        adapterSetting.m_adapterIndex = index_of_adapter;
        memset (adapterSetting.m_name, '\0', sizeof (adapterSetting.m_name));

        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "GetAdapterName", (void *)&adapterSetting, sizeof(adapterSetting));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            strncpy (pNameOfAdapter, adapterSetting.m_name, (BTMGR_NAME_LEN_MAX - 1));
            BTMGRLOG_INFO ("BTMGR_GetAdapterName : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_GetAdapterName : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_SetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char power_status)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterPower_t powerStatus;
    
    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (power_status > 1))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        powerStatus.m_adapterIndex = index_of_adapter;
        powerStatus.m_powerStatus = power_status;

        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "SetAdapterPowerStatus", (void *)&powerStatus, sizeof(powerStatus));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("BTMGR_SetAdapterPowerStatus : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_SetAdapterPowerStatus : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char *pPowerStatus)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterPower_t powerStatus;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pPowerStatus))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        powerStatus.m_adapterIndex = index_of_adapter;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "GetAdapterPowerStatus", (void *)&powerStatus, sizeof(powerStatus));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            *pPowerStatus = powerStatus.m_powerStatus;
            BTMGRLOG_INFO ("BTMGR_GetAdapterPowerStatus : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_GetAdapterPowerStatus : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_SetAdapterDiscoverable(unsigned char index_of_adapter, unsigned char discoverable, unsigned short timeout)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscoverable_t discoverableSetting;
    
    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (discoverable > 1))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        discoverableSetting.m_adapterIndex = index_of_adapter;
        discoverableSetting.m_isDiscoverable = discoverable;
        discoverableSetting.m_timeout = timeout;

        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "SetAdapterDiscoverable", (void *)&discoverableSetting, sizeof(discoverableSetting));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("BTMGR_SetAdapterDiscoverable : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_SetAdapterDiscoverable : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_IsAdapterDiscoverable(unsigned char index_of_adapter, unsigned char *pDiscoverable)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscoverable_t discoverableSetting;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pDiscoverable))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        discoverableSetting.m_adapterIndex = index_of_adapter;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "IsAdapterDiscoverable", (void *)&discoverableSetting, sizeof(discoverableSetting));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            *pDiscoverable = discoverableSetting.m_isDiscoverable;
            BTMGRLOG_INFO ("BTMGR_IsAdapterDiscoverable : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_IsAdapterDiscoverable : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_StartDeviceDiscovery(unsigned char index_of_adapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscover_t deviceDiscovery;

    if (BTMGR_ADAPTER_COUNT_MAX < index_of_adapter)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        deviceDiscovery.m_adapterIndex = index_of_adapter;
        deviceDiscovery.m_setDiscovery = 1; /* TRUE */

        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "SetDeviceDiscoveryStatus", (void *)&deviceDiscovery, sizeof(deviceDiscovery));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("BTMGR_StartDeviceDiscovery : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_StartDeviceDiscovery : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_StopDeviceDiscovery(unsigned char index_of_adapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscover_t deviceDiscovery;

    if (BTMGR_ADAPTER_COUNT_MAX < index_of_adapter)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        deviceDiscovery.m_adapterIndex = index_of_adapter;
        deviceDiscovery.m_setDiscovery = 0; /* FALSE */

        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "SetDeviceDiscoveryStatus", (void *)&deviceDiscovery, sizeof(deviceDiscovery));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("BTMGR_StopDeviceDiscovery : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_StopDeviceDiscovery : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_PairDevice(unsigned char index_of_adapter, const char* pNameOfDevice)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMPairDevice_t deviceName;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pNameOfDevice))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        memset (&deviceName, 0, sizeof(deviceName));
        deviceName.m_adapterIndex = index_of_adapter;
        strncpy (deviceName.m_name, pNameOfDevice, (BTMGR_NAME_LEN_MAX - 1));
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "PairDevice", (void *)&deviceName, sizeof(deviceName));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("BTMGR_PairDevice : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_PairDevice : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_UnpairDevice(unsigned char index_of_adapter, const char* pNameOfDevice)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMPairDevice_t deviceName;
    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pNameOfDevice))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        memset (&deviceName, 0, sizeof(deviceName));
        deviceName.m_adapterIndex = index_of_adapter;
        strncpy (deviceName.m_name, pNameOfDevice, (BTMGR_NAME_LEN_MAX - 1));
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "UnpairDevice", (void *)&deviceName, sizeof(deviceName));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("BTMGR_UnpairDevice : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_UnpairDevice : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetPairedDevices(unsigned char index_of_adapter, BTMGR_Devices_t *pPairedDevices)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMDevices_t pairedDevices;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pPairedDevices))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        memset (&pairedDevices, 0, sizeof(pairedDevices));
        pairedDevices.m_adapterIndex = index_of_adapter;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "GetDiscoveredDevices", (void *)&pairedDevices, sizeof(pairedDevices));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            memcpy (pPairedDevices, &pairedDevices.m_devices, sizeof(BTMGR_Devices_t));
            BTMGRLOG_INFO ("BTMGR_GetPairedDevices : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_GetPairedDevices : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_ConnectToDevice(unsigned char index_of_adapter, const char* pNameOfDevice, BTMGR_Device_Type_t connectAs)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMConnectDevice_t connectToDevice;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pNameOfDevice))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        connectToDevice.m_adapterIndex = index_of_adapter;
        connectToDevice.m_connectAs = connectAs;
        strncpy (connectToDevice.m_name, pNameOfDevice, (BTMGR_NAME_LEN_MAX - 1));
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "ConnectToDevice", (void *)&connectToDevice, sizeof(connectToDevice));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("BTMGR_ConnectToDevice : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_ConnectToDevice : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}


BTMGR_Result_t BTMGR_DisconnectFromDevice(unsigned char index_of_adapter, const char* pNameOfDevice)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMConnectDevice_t disConnectToDevice;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pNameOfDevice))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        disConnectToDevice.m_adapterIndex = index_of_adapter;
        strncpy (disConnectToDevice.m_name, pNameOfDevice, (BTMGR_NAME_LEN_MAX - 1));
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "DisconnectFromDevice", (void *)&disConnectToDevice, sizeof(disConnectToDevice));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("BTMGR_DisconnectFromDevice : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_DisconnectFromDevice : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetDeviceProperties(unsigned char index_of_adapter, const char* pNameOfDevice, BTMGR_DevicesProperty_t *pDeviceProperty)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMDDeviceProperty_t deviceProperty;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pNameOfDevice) || (NULL == pDeviceProperty))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        deviceProperty.m_adapterIndex = index_of_adapter;
        strncpy (deviceProperty.m_name, pNameOfDevice, (BTMGR_NAME_LEN_MAX - 1));
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "DisconnectFromDevice", (void *)&deviceProperty, sizeof(deviceProperty));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            memcpy (pDeviceProperty, &deviceProperty.m_deviceProperty, sizeof(BTMGR_DevicesProperty_t));
            BTMGRLOG_INFO ("BTMGR_GetDeviceProperties : Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_GetDeviceProperties : Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_RegisterEventCallback(BTMGR_EventCallback eventCallback)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    if (!eventCallback)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        m_eventCallbackFunction = eventCallback;
        BTMGRLOG_INFO ("BTMGR_RegisterEventCallback : Success");
    }
    return rc;
}

BTMGR_Result_t BTMGR_SetDefaultDeviceToStreamOut(const char* pNameOfDevice)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    char name[BTMGR_NAME_LEN_MAX];

    if  (NULL == pNameOfDevice)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        memset (name, '\0', sizeof(name));
        strncpy (name, pNameOfDevice, (BTMGR_NAME_LEN_MAX - 1));
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "DeviceToStreamOut", (void *)&name, sizeof(name));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("BTMGR_SetDefaultDeviceToStreamOut: Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_SetDefaultDeviceToStreamOut: Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_SetPreferredAudioStreamOutType (BTMGR_StreamOut_Type_t stream_pref)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMPrefAudioType_t pref;

    pref.m_audioPref = stream_pref;

    retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "SetPreferredAudio", (void *)&pref, sizeof(pref));
    if (IARM_RESULT_SUCCESS == retCode)
    {
        BTMGRLOG_INFO ("BTMGR_SetPreferredAudioStreamOutType: Success");
    }
    else
    {
        rc = BTMGR_RESULT_GENERIC_FAILURE;
        BTMGRLOG_ERROR ("BTMGR_SetPreferredAudioStreamOutType: Failed; RetCode = %d", retCode);
    }
    return rc;
}

BTMGR_Result_t BTMGR_StartAudioStreamingOut(unsigned char index_of_adapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMStartStreaming_t streaming;

    if (BTMGR_ADAPTER_COUNT_MAX < index_of_adapter)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        streaming.m_adapterIndex = index_of_adapter;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "StartAudioStreaming", (void *)&streaming, sizeof(streaming));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("BTMGR_StartAudioStreamingOut: Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_StartAudioStreamingOut: Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_StopAudioStreamingOut()
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

    retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "StopAudioStreaming", 0, 0);
    if (IARM_RESULT_SUCCESS == retCode)
    {
        BTMGRLOG_INFO ("BTMGR_StopAudioStreamingOut: Success");
    }
    else
    {
        rc = BTMGR_RESULT_GENERIC_FAILURE;
        BTMGRLOG_ERROR ("BTMGR_StopAudioStreamingOut: Failed; RetCode = %d", retCode);
    }
    return rc;
}

BTMGR_Result_t BTMGR_IsAudioStreamingOut(unsigned char index_of_adapter, unsigned char *pStreamingStatus)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMStreamingStatus_t status;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pStreamingStatus))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        status.m_adapterIndex = index_of_adapter;
        status.m_streamingStatus = 0;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "IsAudioStreaming", (void *)&status, sizeof(status));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            *pStreamingStatus = status.m_streamingStatus;
            BTMGRLOG_INFO ("BTMGR_IsAudioStreamingOut: Success");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("BTMGR_IsAudioStreamingOut: Failed; RetCode = %d", retCode);
        }
    }
    return rc;
}
/**********************/
/* Private Interfaces */
/**********************/
static void _btmgr_deviceCallback(const char *owner, IARM_EventId_t eventId, void *pData, size_t len)
{
    if (NULL == pData)
    {
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        BTMGR_EventMessage_t *pReceivedEvent = (BTMGR_EventMessage_t*)pData;
        BTMGR_EventMessage_t newEvent;

        if ((BTMGR_IARM_EVENT_DEVICE_OUT_OF_RANGE == eventId)   ||
            (BTMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE == eventId) ||
            (BTMGR_IARM_EVENT_DEVICE_DISCOVERY_COMPLETE == eventId) ||
            (BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST == eventId) ||
            (BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST == eventId))
        {
            memcpy (&newEvent, pReceivedEvent, sizeof(BTMGR_EventMessage_t));
            m_eventCallbackFunction (newEvent);
            BTMGRLOG_INFO ("_btmgr_deviceCallback : posted event(%d) from the adapter(%d) to listener successfully", newEvent.m_eventType, newEvent.m_adapterIndex);
        }
        else
        {
            BTMGRLOG_ERROR ("%s : Event is invalid", __FUNCTION__);
        }
    }
}


#endif /* BTMGR_ENABLE_IARM_INTERFACE */
