/*! @file GPMF_demo.c
 *
 *  @brief Demo to extract GPMF from an MP4
 *
 *  @version 1.5.0
 *
 *  (C) Copyright 2017 GoPro Inc (http://gopro.com/).
 *	
 *  Licensed under either:
 *  - Apache License, Version 2.0, http://www.apache.org/licenses/LICENSE-2.0  
 *  - MIT license, http://opensource.org/licenses/MIT
 *  at your option.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include <array>
#include <vector>
#include <iostream>
#include <iomanip>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "../GPMF_parser.h"
#include "GPMF_mp4reader.h"

using namespace std;

class TD {
public:
	TD() {
			year = 2000;
			month = day = 1;
			hour = 12;
			minute = second = 0;
	};
	~TD() {};
	friend ostream& operator<<(ostream& os, const TD& dt)
	{
	    os  << dt.year << '-' 
	    	<< std::setfill('0') << std::setw(2) << dt.month << '-' 
	    	<< std::setfill('0') << std::setw(2) << dt.day << 'T'
	    	<< std::setfill('0') << std::setw(2) << dt.hour << ':'
	    	<< std::setfill('0') << std::setw(2) << dt.minute << ':'
	    	<< std::setfill('0') << std::setw(2) << dt.second << 'Z';
	    return os;
	};
	void readGPMeta(char* gp) {
				   	// UTC Time format yymmddhhmmss.sss 
				   	year  = 2000 + 10*(gp[0]-'0') + (gp[1]-'0');
				   	month = 10*(gp[2]-'0') + (gp[3]-'0');
				   	day   = 10*(gp[4]-'0') + (gp[5]-'0');
				   	hour  = 10*(gp[6]-'0') + (gp[7]-'0');
				   	minute= 10*(gp[8]-'0') + (gp[9]-'0');
				   	second= 10*(gp[10]-'0')+ (gp[11]-'0');		
	};
	int getSeconds() { return second; };
protected:
	int year, month, day, hour, minute, second;
};

int main(int argc, char *argv[])
{
	int32_t ret = GPMF_OK;
	GPMF_stream metadata_stream, *ms = &metadata_stream;
	double metadatalength;
	double start_offset = 0.0;
	uint32_t *payload = NULL; //buffer to store GPMF samples from the MP4.
	std::vector<float> lat;
	std::vector<float> lon;
	std::vector<float> ele;
	TD currentTime;


	// get file return data
	if (argc != 2)
	{
		printf("usage: %s <file_with_GPMF>\n", argv[0]);
		return -1;
	}

	size_t mp4 = OpenMP4Source(argv[1], MOV_GPMF_TRAK_TYPE, MOV_GPMF_TRAK_SUBTYPE);
	if (mp4 == 0)
	{
		printf("error: %s is an invalid MP4/MOV or it has no GPMF data\n", argv[1]);
		return -1;
	}


	metadatalength = GetDuration(mp4);

	if (metadatalength > 0.0)
	{
		uint32_t index, payloads = GetNumberPayloads(mp4);
//		printf("found %.2fs of metadata, from %d payloads, within %s\n", metadatalength, payloads, argv[1]);

		uint32_t fr_num, fr_dem;
		uint32_t frames = GetVideoFrameRateAndCount(mp4, &fr_num, &fr_dem);
		if (frames)
		{
			printf("video framerate is %.2f with %d frames\n", (float)fr_num/(float)fr_dem, frames);
		}

		for (index = 0; index < payloads; index++)
		{
			uint32_t payloadsize = GetPayloadSize(mp4, index);
			double in = 0.0, out = 0.0; //times
			payload = GetPayload(mp4, payload, index);
			if (payload == NULL)
				goto cleanup;

			ret = GetPayloadTime(mp4, index, &in, &out);
			if (ret != GPMF_OK)
				goto cleanup;

			ret = GPMF_Init(ms, payload, payloadsize);
			if (ret != GPMF_OK)
				goto cleanup;


#if 1		// Find GPS values and return scaled doubles. 
			if (index >= 0) // show first payload 
			{

#if 0
				if (GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("GPSU"), GPMF_RECURSE_LEVELS)) //GoPro Hero5/6/7 GPS
				{
					char* pUTC;
					char pDest[17];
					static int priorSeconds=-1;

				   	uint32_t key = GPMF_Key(ms);
				   	char type = GPMF_Type(ms);
				   	uint32_t samples = GPMF_Repeat(ms);
				   	pUTC = (char*)GPMF_RawData(ms);
				   	strncpy(pDest, pUTC, 16);
				   	pDest[16] = 0;
//				   	printf("GPSU Time Buffer Raw: %s\n", pDest);

				   	// UTC Time format yymmddhhmmss.sss 
				   	currentTime.readGPMeta(pUTC);

				   	cout << "GPSU Decoded: DateTime: " << currentTime << endl;

				   	if (priorSeconds == currentTime.getSeconds()) 
				   		printf("***DUPLICATE*** Time is the same as last time (within one second)\n");

				   	priorSeconds = currentTime.getSeconds();
				}
				else
#endif
				if (GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("GPS5"), GPMF_RECURSE_LEVELS)) //GoPro Hero5/6/7 GPS
				{
					uint32_t key = GPMF_Key(ms);
					uint32_t samples = GPMF_Repeat(ms);
					uint32_t elements = GPMF_ElementsInStruct(ms);
					uint32_t buffersize = samples * elements * sizeof(double);
					GPMF_stream find_stream;
					double *ptr, *tmpbuffer = (double *)malloc(buffersize);
					char units[10][6] = { "" };
					uint32_t unit_samples = 1;

					printf("MP4 Payload time %.3f to %.3f seconds\n", in, out);

					if (tmpbuffer && samples)
					{
						uint32_t i, j;

						//Search for any units to display
						GPMF_CopyState(ms, &find_stream);
						if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_SI_UNITS, GPMF_CURRENT_LEVEL) ||
							GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_UNITS, GPMF_CURRENT_LEVEL))
						{
							char *data = (char *)GPMF_RawData(&find_stream);
							int ssize = GPMF_StructSize(&find_stream);
							unit_samples = GPMF_Repeat(&find_stream);

							for (i = 0; i < unit_samples; i++)
							{
								memcpy(units[i], data, ssize);
								units[i][ssize] = 0;
								data += ssize;
							}
						}

						//GPMF_FormattedData(ms, tmpbuffer, buffersize, 0, samples); // Output data in LittleEnd, but no scale
						GPMF_ScaledData(ms, tmpbuffer, buffersize, 0, samples, GPMF_TYPE_DOUBLE);  //Output scaled data as floats

						ptr = tmpbuffer;
						for (i = 0; i < samples; i++)
						{
							float fLat = *ptr;
							float fLon = *(ptr+1);
							float fEle = *(ptr+2);
							// Time frequency is 1Hz - Hero 5 GPS5 freq is 18Hz
							// Only take the first sample of any set of samples.
							if (i==0) {
								printf("GPS: %.3f, %.3f @ %.3f\n", fLat, fLon, fEle);
								lat.push_back(*ptr);
								lon.push_back(*(ptr+1));
								ele.push_back(*(ptr+2));
							}

//							printf("%c%c%c%c ", PRINTF_4CC(key));
//							for (j = 0; j < elements; j++)
//								printf("%.3f%s, ", *ptr++, units[j%unit_samples]);

//							printf("\n");
						}
						free(tmpbuffer);
					}
				}
				GPMF_ResetState(ms);
				printf("\n");
			}
#endif 
		}

	printf("\nSUMMARY: # of GPS points: %lu\n", lat.size());

	cleanup:
		if (payload) FreePayload(payload); payload = NULL;
		CloseSource(mp4);
	}

	return ret;
}
