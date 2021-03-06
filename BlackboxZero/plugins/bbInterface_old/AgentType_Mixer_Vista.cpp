#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <Functiondiscoverykeys_devpkey.h>
#include "AgentMaster.h"
#include "ControlMaster.h"
#include "Definitions.h"
#include "MenuMaster.h"
#include "BBApi.h"
#include "ConfigMaster.h"
#include "AgentType_Mixer_Vista.h"

//@TODO: place defaultDevice endpoint as first
//@TODO: use bb unicode routines
//@TODO: get mute (bool) working

int agenttype_mixer_startup_vista ()
{
	CoInitialize(NULL);
	//Register this type with the ControlMaster
	agent_registertype(
		"Mixer",                            //Friendly name of agent type
		"MixerScale",                       //Name of agent type
		CONTROL_FORMAT_SCALE|CONTROL_FORMAT_TEXT,               //Control format
		true,
		&agenttype_mixer_create_vista,
		&agenttype_mixer_destroy_vista,
		&agenttype_mixer_message_vista,
		&agenttype_mixer_notify_vista,
		&agenttype_mixer_getdata_vista,
		&agenttype_mixerscale_menu_set_vista,
		&agenttype_mixer_menu_context_vista,
		&agenttype_mixer_notifytype_vista
		);

	agent_registertype(
		"Mixer",                            //Friendly name of agent type
		"MixerBool",                        //Name of agent type
		CONTROL_FORMAT_BOOL,                //Control format
		true,
		&agenttype_mixer_create_vista,
		&agenttype_mixer_destroy_vista,
		&agenttype_mixer_message_vista,
		&agenttype_mixer_notify_vista,
		&agenttype_mixer_getdata_vista,
		&agenttype_mixerbool_menu_set_vista,
		&agenttype_mixer_menu_context_vista,
		&agenttype_mixer_notifytype_vista
		);
	return 0;
}

int agenttype_mixer_shutdown_vista ()
{
	CoUninitialize();
	return 0;
}

class CVolumeNotification;

struct AgentType_Mixer_Vista
{
	AgentType_Mixer_Vista (char const * device)
		: m_endpoint(0)
		, m_agent(0)
		, m_callback(0)
		, m_value_double(0.0)
		, m_value_bool(false)
	{
		size_t const n = strlen(device);
		strncpy(m_device, device, n);
		m_device[n] = 0;
	}

	bool Init ();
	bool MkCallback ();
	void Destroy ();

	float GetVolume () const;
	void SetVolume (float v) const;

	bool GetMute () const;
	void SetMute (bool m) const;

	char m_device[1024];
	IAudioEndpointVolume * m_endpoint;
	agent * m_agent;
	CVolumeNotification * m_callback;
	double m_value_double;
	bool m_value_bool;
	char m_value_text[32];
};

int agenttype_mixer_create_vista (agent * a, char * parameterstring)
{
	bool errorflag = false; //If there's an error

	AgentType_Mixer_Vista * details = new AgentType_Mixer_Vista(parameterstring);
	if (details->Init())
	{
		details->m_agent = a;
		a->agentdetails = static_cast<void *>(details);
	}
	else
	{
		details->Destroy();
		delete details;
		a->agentdetails = NULL;

		agent_destroy(&a);
		return 1;   
	}
	return 0;
}

int agenttype_mixer_destroy_vista (agent * a)
{
	if (a->agentdetails)
	{
		AgentType_Mixer_Vista * details = static_cast<AgentType_Mixer_Vista *>(a->agentdetails);
		details->Destroy();
		delete details;
		a->agentdetails = NULL;
	}
	return 0;
}

int agenttype_mixer_message_vista (agent *a, int tokencount, char *tokens[])
{
	return 1;
}

void agenttype_mixer_notify_vista (agent *a, int notifytype, void *messagedata)
{
	if (a->agentdetails)
	{
		AgentType_Mixer_Vista * const details = static_cast<AgentType_Mixer_Vista *>(a->agentdetails);

		switch (notifytype)
		{
			case NOTIFY_CHANGE:
			{
				double * value_double = static_cast<double *>(messagedata);
				if (value_double)
				{
					float volume = static_cast<float>(*value_double);
					details->SetVolume(volume);
				}
				break;
			}
			case NOTIFY_SAVE_AGENT:
			{
				char temp[1024];
				sprintf(temp, "%s", details->m_device);
				config_write(config_get_control_setagent_c(a->controlptr, a->agentaction, a->agenttypeptr->agenttypename, temp));
				break;
			}
		}
	}
}

void * agenttype_mixer_getdata_vista (agent *a, int datatype)
{
	if (a->agentdetails)
	{
		AgentType_Mixer_Vista * details = static_cast<AgentType_Mixer_Vista *>(a->agentdetails);

		details->m_value_double = details->GetVolume();
		details->m_value_bool = details->GetMute();

		switch (datatype)
		{
			case DATAFETCH_VALUE_TEXT:
			{
				int const intvalue = static_cast<int>(100.0 * details->m_value_double);
				sprintf(details->m_value_text, "%d%%", intvalue);
				return details->m_value_text;
			}
			case DATAFETCH_VALUE_SCALE:
				return &(details->m_value_double);
			case DATAFETCH_VALUE_BOOL:
				return &(details->m_value_bool);
		}
	}
	return NULL;
}

void agenttype_mixer_menu_devices_vista (Menu *menu, control *c, char *action, char *agentname, int format);
void agenttype_mixerscale_menu_set_vista (Menu *m, control *c, agent *a,  char *action, int controlformat)
{
	agenttype_mixer_menu_devices_vista(m, c, action, "MixerScale", CONTROL_FORMAT_SCALE);
}
void agenttype_mixerbool_menu_set_vista (Menu *m, control *c, agent *a,  char *action, int controlformat)
{
	agenttype_mixer_menu_devices_vista(m, c, action, "MixerBool", CONTROL_FORMAT_BOOL);
}

void agenttype_mixer_menu_context_vista (Menu *m, agent *a)
{
	make_menuitem_nop(m, "No options available.");
}

void agenttype_mixer_notifytype_vista (int notifytype, void *messagedata)
{
}

///////////////////////////////////////////////////////////////////////////////
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
namespace {
	//@TODO: i found bbWC2MB in Tray.cpp .. use these ones if suitable
	// Convert an UTF8 string to a wide Unicode String
	int utf8_encode (wchar_t const * wstr, char * buff, size_t buff_sz)
	{
		size_t const w_size = wcslen(wstr);
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, w_size, NULL, 0, NULL, NULL);
		if (size_needed > buff_sz)
			size_needed = buff_sz;
		int const n = WideCharToMultiByte(CP_UTF8, 0, wstr, w_size, buff, size_needed, NULL, NULL);
		if (n == buff_sz)
			buff[n - 1] = NULL;
		else
			buff[n] = NULL;
		return size_needed;
	}
	// Convert an UTF8 string to a wide Unicode String
	int utf8_decode (char const * str, wchar_t * buff, size_t buff_sz)
	{
		size_t const size = strlen(str);
		int size_needed = MultiByteToWideChar(CP_UTF8, 0, str, size, NULL, 0);
		if (size_needed > buff_sz)
			size_needed = buff_sz;
		int const n = MultiByteToWideChar(CP_UTF8, 0, str, size, buff, size_needed);
		if (n == buff_sz)
			buff[n - 1] = NULL;
		else
			buff[n] = NULL;
		return size_needed;
	}
}

class CVolumeNotification : public IAudioEndpointVolumeCallback
{
	LONG m_RefCount;
	AgentType_Mixer_Vista & m_details;

	~CVolumeNotification () { }
public:
	CVolumeNotification (AgentType_Mixer_Vista & details) : m_RefCount(1), m_details(details) { }

	STDMETHODIMP_(ULONG) AddRef () { return InterlockedIncrement(&m_RefCount); }
	STDMETHODIMP_(ULONG) Release ()
	{
		LONG ref = InterlockedDecrement(&m_RefCount);
		if (ref == 0)
			delete this;
		return ref;
	}
	STDMETHODIMP QueryInterface (REFIID IID, void ** ReturnValue)
	{
		if (IID == IID_IUnknown || IID== __uuidof(IAudioEndpointVolumeCallback))
		{
			*ReturnValue = static_cast<IUnknown*>(this);
			AddRef();
			return S_OK;
		}
		*ReturnValue = NULL;
		return E_NOINTERFACE;
	}

	STDMETHODIMP OnNotify (PAUDIO_VOLUME_NOTIFICATION_DATA NotificationData)
	{
		//@TODO: check if OnNotify called from SetVolume?
		control_notify(m_details.m_agent->controlptr, NOTIFY_NEEDUPDATE, NULL);
		return S_OK;
	}
};

bool AgentType_Mixer_Vista::MkCallback ()
{
	m_callback = new CVolumeNotification(*this);
	if (S_OK == m_endpoint->RegisterControlChangeNotify(m_callback))
	{
		return true;
	}
	m_callback->Release();
	m_callback = NULL;
	return false;
}

bool AgentType_Mixer_Vista::Init ()
{
	IMMDeviceEnumerator * deviceEnumerator = NULL;
	if (S_OK == CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, IID_IMMDeviceEnumerator, (LPVOID *)&deviceEnumerator))
	{
		// find device from config
		wchar_t tmp[1024];
		utf8_decode(m_device, tmp, 1024);
		IMMDevice * device = 0;
		if (S_OK == deviceEnumerator->GetDevice(tmp, &device))
		{
			IAudioEndpointVolume * endpointVolume = NULL;
			if (S_OK == device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, NULL, (LPVOID *)&endpointVolume))
			{
				device->Release();
				device = NULL;
				m_endpoint = endpointVolume;
				MkCallback();
				return true;
			}
		}

		// fallback to default
		IMMDevice * defaultDevice = NULL;
		if (S_OK == deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice))
		{
			deviceEnumerator->Release();
			deviceEnumerator = NULL;

			IAudioEndpointVolume * endpointVolume = NULL;
			if (S_OK == defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_INPROC_SERVER, NULL, (LPVOID *)&endpointVolume))
			{
				defaultDevice->Release();
				defaultDevice = NULL;
				m_endpoint = endpointVolume;
				MkCallback();
				return true;
			}
		}
	}
	return false;
}

//@TODO: remove
#define SAFE_RELEASE(punk)  if ((punk) != NULL)  { (punk)->Release(); (punk) = NULL; }
extern const char * mixer_name_scale;
extern const char * mixer_name_bool;

void agenttype_mixer_menu_devices_vista (Menu *menu, control *c, char *action, char *agentname, int format)
{
	IMMDeviceEnumerator * pEnumerator = NULL;
	IMMDeviceCollection * pCollection = NULL;
	IMMDevice * pEndpoint = NULL;

	if (S_OK == CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator))
	{
		if (S_OK == pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection))
		{
			UINT  count = 0;
			if (S_OK ==  pCollection->GetCount(&count))
			{
				for (ULONG i = 0; i < count; i++)
				{
					if (S_OK != pCollection->Item(i, &pEndpoint))
						continue;

					LPWSTR pwszID = NULL;
					if (S_OK != pEndpoint->GetId(&pwszID))
						continue;

					char id_tmp[1024];
					utf8_encode(pwszID, id_tmp, 1024);

					IPropertyStore * pProps = NULL;
					if (S_OK == pEndpoint->OpenPropertyStore(STGM_READ, &pProps))
					{
						PROPVARIANT varName;
						PropVariantInit(&varName);
						if (S_OK == pProps->GetValue(PKEY_Device_FriendlyName, &varName))
						{
							char tmp[1024];
							utf8_encode(varName.pwszVal, tmp, 1024);

							const char * type = 0;
							if (format == CONTROL_FORMAT_SCALE)
								type = mixer_name_scale;
							else if (format == CONTROL_FORMAT_BOOL)
								type = mixer_name_bool;

							char text_item[256];
							char text_params[256];
							sprintf(text_item, "%s", tmp);
							sprintf(text_params, "%s", id_tmp);
							make_menuitem_cmd(menu, text_item, config_getfull_control_setagent_c(c, action, type, text_params));

							PropVariantClear(&varName);
						}
					}

					CoTaskMemFree(pwszID);
					pwszID = NULL;
					SAFE_RELEASE(pProps)
					SAFE_RELEASE(pEndpoint)
				}
			}
		}
	}
	SAFE_RELEASE(pEnumerator)
	SAFE_RELEASE(pCollection)
	return;
}

#undef SAFE_RELEASE

float AgentType_Mixer_Vista::GetVolume () const
{
	float currentVolume = 0.0f;
	if (m_endpoint && S_OK == m_endpoint->GetMasterVolumeLevelScalar(&currentVolume))
	{
		return currentVolume;
	}
	return 0.0f;
}

void AgentType_Mixer_Vista::SetVolume (float v) const
{
	if (m_endpoint)
		m_endpoint->SetMasterVolumeLevelScalar(v, NULL);
}

void AgentType_Mixer_Vista::Destroy ()
{
	if (m_endpoint)
	{
		if (m_callback)
		{
			m_endpoint->UnregisterControlChangeNotify(m_callback); 
			m_callback->Release();
			m_callback = NULL;
		}
		m_endpoint->Release();
		m_endpoint = NULL;
	}
}

bool AgentType_Mixer_Vista::GetMute () const
{
	BOOL mute = 0;
	if (m_endpoint && S_OK == m_endpoint->GetMute(&mute))
	{
		return mute;
	}
	return false;
}

void AgentType_Mixer_Vista::SetMute (bool m) const
{
	if (m_endpoint)
		m_endpoint->SetMute(m, NULL);
}

