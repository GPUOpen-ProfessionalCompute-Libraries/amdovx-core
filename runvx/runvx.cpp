/* 
Copyright (c) 2015 Advanced Micro Devices, Inc. All rights reserved.
 
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
 
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
 
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


#define _CRT_SECURE_NO_WARNINGS

#if _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include "vxParamHelper.h"
#include "vxEngineUtil.h"
#include "vxEngine.h"

// version
#define RUNVX_VERSION "0.9.0"

class CFileBuffer {
public:
	CFileBuffer(const char * fileName) {
		size_in_bytes = 0; buffer_allocated = buffer_aligned = 0;
		FILE * fp = fopen(fileName, "rb");
		if (!fp) {
			printf("ERROR: unable to open '%s'\n", fileName);
		}
		else {
			fseek(fp, 0L, SEEK_END); size_in_bytes = ftell(fp); fseek(fp, 0L, SEEK_SET);
			buffer_allocated = new unsigned char[size_in_bytes + 32];
			buffer_aligned = (unsigned char *)((((intptr_t)buffer_allocated) + 31) & ~31);
			(void)fread(buffer_aligned, 1, size_in_bytes, fp);
			buffer_aligned[size_in_bytes] = 0;
			//printf("OK: read %d bytes from %s\n", size_in_bytes, fileName);
			fclose(fp);
		}
	}
	CFileBuffer(size_t _size_in_bytes, size_t _prefix_bytes = 0, size_t _postfix_bytes = 0) {
		size_in_bytes = _size_in_bytes;
		prefix_bytes = _prefix_bytes;
		postfix_bytes = _postfix_bytes;
		buffer_allocated = new unsigned char[size_in_bytes + prefix_bytes + postfix_bytes + 32];
		buffer_aligned = (unsigned char *)((((intptr_t)(buffer_allocated + prefix_bytes)) + 31) & ~31);
		memset(buffer_aligned, 0, size_in_bytes);
	}
	~CFileBuffer() { if (buffer_allocated) delete[] buffer_allocated; }
	void * GetBuffer() { return buffer_aligned; }
	size_t GetSizeInBytes() { return size_in_bytes; }
	int WriteFile(const char * fileName) {
		if (!buffer_aligned) return -1;
		FILE * fp = fopen(fileName, "wb"); if (!fp) { printf("ERROR: unable to open '%s'\n", fileName); return -1; }
		fwrite(buffer_aligned, 1, size_in_bytes, fp); fclose(fp);
		printf("OK: wrote %d bytes into %s\n", (int)size_in_bytes, fileName);
		return 0;
	}
private:
	unsigned char * buffer_allocated, *buffer_aligned;
	size_t size_in_bytes, prefix_bytes, postfix_bytes;
};


void show_usage(const char * program, bool detail)
{
	printf("RUNVX.EXE %s\n", RUNVX_VERSION);
	printf("\n");
	printf("Usage:\n");
	printf("  runvx.exe [options] file <GDF-file> argument(s)\n");
	printf("  runvx.exe [options] node <kernelName> argument(s)\n");
	printf("\n");
	printf("[options]:\n");
	printf("  -h                                 -- show full help\n");
	printf("  -v                                 -- verbose\n");
	printf("  -root:<directory>                  -- replace ~ in filenames with <directory> in the command-line and\n");
	printf("                                        GDF file.The default value of '~' is '.' when\n");
	printf("                                        this option is not specified.\n");
	printf("  -frames:[<start>[,<end>]]/[live]   -- run the graph/node for specified frames\n");
	printf("  -frames-or-eof::[<start>[,<end>]]  -- run the graph/node for specified frames or until eof\n");
	printf("  -affinity:<type>                   -- set default target affinity to <type> (shall be CPU or GPU)\n");
	printf("  -ago-profile                       -- print node-level performance data for profiling\n");
	printf("  -no-abort-on-mismatch              -- continue graph processing even if a mismatch occurs\n");
	printf("  -no-run                            -- abort after vxVerifyGraph\n");
	printf("  -disable-virtual                   -- replace all virtual data types in GDF with non-virtual data types\n");

	if (!detail) return;
	printf("\n");
	printf("argument(s):\n");
	printf("  GDF objects can be created on command-line with I/O capability and passed into a GDF file.\n");
	printf("  A GDF file can access these GDF objects using $1, $2, $3, etc. corresponding to its position\n");
	printf("  command-line. In addition to read/write/compare/initialize, image objects can be connected to\n");
	printf("  a camera, image and keypoint-array objects can be displayed in a window. See below for argument\n");
	printf("  specification, which is an extension to GDF object syntax for limited set of GDF objects.\n");
	printf("\n");
	printf("  image:<width>,<height>,<format>[[:<io-params>]...]\n");
	printf("  image-virtual:<width>,<height>,<image-format>\n");
	printf("  image-uniform:<width>,<height>,<image-format>,<uniform-pixel-value>[[:<io-params>]...] \n");
	printf("    <image-format>         -   four letter string with values of VX_DF_IMAGE (see vx_df_image_e in vx_types.h)\n");
	printf("                               some useful formats are: RGB2,RGBX,IYUV,NV12,U008,S016,U032,U001,F032,...\n");
	printf("    <uniform-pixel-val>    -   pixel value for uniform image\n");
	printf("    <io-params>            -   READ,<fileNameorURL>[,frames{<start>[;<count>;repeat]}] or camera,deviceNumber\n");
	printf("                                   to read input from file, URL, camera \n");
	printf("                               VIEW,<window-name> to display in a window\n");
	printf("                               WRITE,<fileNameOrURL> to write\n");
	printf("                               COMPARE,<fileName>[,rect{<start-x>;<start-y>;<end-x>;<end-y>}][,err{<min>;<max>}]\n");
	printf("                                                 [,checksum|checksum-save-instead-of-test]\n");
	printf("                                   to compare output for exact-match or md5 checksum match or match\n");
	printf("                                   with range of pixel values, or generate md5 checksum without comparing\n");
	printf("\n");
	printf("  array:<format>,<capacity>[[:<io-params>]...]\n");
	printf("  array-virtual:<format>,<capacity>[[:<io-params>]...]\n");
	printf("    <format>               -   KEYPOINT/COORDINATES2D/COORDINATES3D/RECTANGLE\n");
	printf("    <io-params>            -   READ,<fileName>[,ascii|binary] to read \n");
	printf("                               VIEW,<window-name> to display points overlaid in a window\n");
	printf("                               WRITE,<fileName>[,ascii|binary] to write\n");
	printf("                               COMPARE,<fileName>[,ascii|binary][,err{<x>;<y>[;<strength>]}]\n");
	printf("\n");
	printf("  pyramid:<numLevels>,half|orb|<scale-factor>,<width>,<height>,<image-format>[:<io-params>]\n");
	printf("  pyramid-virtual:<numLevels>,half|orb|<scale-factor>,<width>,<height>,<image-format>\n");
	printf("    <numLevels>            -   number of levels of the pyramid\n");
	printf("    <image-format>         -   four letter string with values of VX_DF_IMAGE (see vx_df_image_e in vx_types.h)\n");
	printf("                               some useful formats are: U008,S016,...\n");
	printf("    <io-params>            -   READ,<fileName[%%n]> read image into pyramid at level n\n");
	printf("                               WRITE,<fileName[%%n]> write level n to file\n");
	printf("                               COMPARE,<fileName>[,rect{<start-x>;<start-y>;<end-x>;<end-y>}][,err{<min>;<max>}]\n");
	printf("                                                 [,checksum|checksum-save-instead-of-test]\n");
	printf("                                   to compare output for exact-match or md5 checksum match or match\n");
	printf("                                   with range of pixel values, or generate md5 checksum without comparing\n");
	printf("\n");
	printf("  distribution:<numBins>,<offset>,<range>[[:<io-params>]...]\n");
	printf("    <numBins>              -   num of bins\n");
	printf("    <offset>               -   start offset value\n");
	printf("    <range>                -   end value to use as range\n");
	printf("    <io-params>            -   READ,<fileName>[,ascii|binary] to read \n");
	printf("                               VIEW,<window-name> to overlay distribution visualization on a window\n");
	printf("                               WRITE,<fileName>[,ascii|binary] to write\n");
	printf("                               COMPARE,<fileName>[,ascii|binary] exact match compare\n");
	printf("\n");
	printf("  convolution:<columns>,<rows>[[:<io-params>]...]\n");
	printf("    <io-params>            -   READ,<fileName>[,ascii|binary|scale] to read \n");
	printf("                               WRITE,<fileName>[,ascii|binary|scale] to write\n");
	printf("                               COMPARE,<fileName>[,ascii|binary|scale] exact match compare\n");
	printf("                               SCALE,<scale-factor> scale value of the convolution\n");
	printf("                               INIT,{<value1>;<value2>;...<valueN>} initialize matrix to immediate value\n");
	printf("\n");
	printf("  lut:<data-type>,<count>[[:<io-params>]...]\n");
	printf("    <data-type>            -   useful data types are: UINT8\n");
	printf("    <io-params>            -   READ,<fileName>[,ascii|binary] to read \n");
	printf("                               WRITE,<fileName>[,ascii|binary] to write\n");
	printf("                               COMPARE,<fileName>[,ascii|binary] exact match compare\n");
	printf("                               VIEW,<window-name> to view lookup table on a window\n");
	printf("\n");
	printf("  matrix:<data-type>,<columns>,<rows>[:<mode>,<fileName>]\n");
	printf("    <data-type>            -   useful data types are: INT32, FLOAT32\n");
	printf("    <io-params>            -   READ,<fileName>[,ascii|binary] to read \n");
	printf("                               WRITE,<fileName>[,ascii|binary] to write\n");
	printf("                               COMPARE,<fileName>[,ascii|binary] exact match compare\n");
	printf("                               INIT,{<value1>;<value2>;...<valueN>} initialize matrix to immediate value\n");
	printf("\n");
	printf("  remap:<srcWidth>,<srcHeight>,<dstWidth>,<dstHeight>[:<io-params>]\n");
	printf("    <io-params>            -   READ,<fileName>[,ascii|binary] to read \n");
	printf("                               WRITE,<fileName>[,ascii|binary] to write\n");
	printf("                               COMPARE,<fileName>[,ascii|binary][,err{<x>;<y>}] compare within range\n");
	printf("                               INIT,<filename>|rotate-90|rotate-180|rotate-270|scale|hflip|vflip\n");
	printf("                                   initialize remap table with file or in-built remaps\n");
	printf("\n");
	printf("  scalar:<data-type>,<value>[:<io-params>]\n");
	printf("    <data-type>            -   any scalar datatype supported by OpenVX\n");
	printf("                               some useful data types are: INT32, UINT32, FLOAT32, ENUM, BOOL, SIZE, ...\n");
	printf("    <io-params>            -   READ,<fileName> to read from file\n");
	printf("                               WRITE,<fileName> to write to file\n");
	printf("                               COMPARE,<fileName>[,range] compare within range in file,separated by space\n");
	printf("                               VIEW,<window-name> to view scalar value on a window\n");
	printf("\n");
	printf("  threshold:<thresh-type>,<data-type>[:<io-params>]\n");
	printf("    <thresh-type>          -   BINARY/RANGE\n");
	printf("    <data-type>            -   useful data types are: UINT8, INT16\n");
	printf("    <io-params>            -   READ,<fileName> to read from file\n");
	printf("                               INIT,<value1>[,<value2>] threshold value or range\n");
}

int main(int argc, char * argv[])
{	
	// process command-line options
	const char * program = "runvx.exe";
	bool verbose = false;
	bool enableMultiFrameProcessing = false;
	bool framesEofRequested = true;
	bool useAgoImport = false, useAgoDump = false, doRun = true, doUseProcessGraph = false;
	bool doPause = false;
	bool doGetInternalProfile = false;
	bool disableVirtual = false;
	bool abortOnMismatch = true;
	vx_uint32 defaultTargetAffinity = 0;
	vx_uint32 defaultTargetInfo = 0;
	bool doSetGraphOptimizerFlags = false;
	vx_uint32 graphOptimizerFlags = 0;
	int arg, frameStart = 0, frameEnd = 1;
	bool frameCountSpecified = false;
	for (arg = 1; arg < argc; arg++){
		if (argv[arg][0] == '-'){
			if (!strcmp(argv[arg], "-h")) {
				show_usage(program, true);
				exit(0);
			}
			else if (!strcmp(argv[arg], "-v")) {
				verbose ^= true;
			}
			else if (!strncmp(argv[arg], "--", 2)) { // skip specified number of arguments: --[#] (default just skip --)
				arg += atoi(&argv[arg][2]);
			}
			else if (!strncmp(argv[arg], "-root:", 6)) {
				SetRootDir(argv[arg] + 6);
			}
			else if (!strncmp(argv[arg], "-frames:", 8) || !strncmp(argv[arg], "-frames-or-eof:", 15)) {
				int spos = 8;
				framesEofRequested = false;
				if (!strncmp(argv[arg], "-frames-or-eof:", 15)) {
					spos = 15;
					framesEofRequested = true;
				}
				if (!strcmp(&argv[arg][spos], "live")) {
					enableMultiFrameProcessing = true;
				}
				else {
					int k = sscanf(&argv[arg][spos], "%d,%d", &frameStart, &frameEnd);
					if (k == 1) { frameEnd = frameStart, frameStart = 0; }
					else if (k != 2) { printf("ERROR: invalid -frames option\n"); return -1; }
				}
				frameCountSpecified = true;
			}
			else if (!_strnicmp(argv[arg], "-affinity:", 10)) {
				if (!_strnicmp(&argv[arg][10], "cpu", 3)) defaultTargetAffinity = AGO_TARGET_AFFINITY_CPU;
				else if (!_strnicmp(&argv[arg][10], "gpu", 3)) defaultTargetAffinity = AGO_TARGET_AFFINITY_GPU;
				else { printf("ERROR: unsupported affinity target: %s\n", &argv[arg][10]); return -1; }
				if (argv[arg][13] >= '0' && argv[arg][13] <= '9')
					defaultTargetInfo = atoi(&argv[arg][13]);
			}
			else if (!strcmp(argv[arg], "-pause")) {
				doPause = true;
			}
			else if (!strcmp(argv[arg], "-ago-profile")) {
				doGetInternalProfile = true;
			}
			else if (!strcmp(argv[arg], "-ago-import")) {
				useAgoImport = true;
			}
			else if (!strcmp(argv[arg], "-no-ago-import")) {
				useAgoImport = false;
			}
			else if (!strcmp(argv[arg], "-ago-dump")) {
				useAgoDump = true;
			}
			else if (!strcmp(argv[arg], "-no-run")) {
				doRun = false;
			}
			else if (!strcmp(argv[arg], "-no-abort-on-mismatch")) {
				abortOnMismatch = false;
			}
			else if (!strcmp(argv[arg], "-use-process-graph")) {
				doUseProcessGraph = true;
			}
			else if (!_stricmp(argv[arg], "-disable-virtual")) {
				disableVirtual = true;
			}
			else if (!_strnicmp(argv[arg], "-graph-optimizer-flags:", 23)) {
				if (sscanf(&argv[arg][23], "%i", &graphOptimizerFlags) == 1) {
					doSetGraphOptimizerFlags = true;
				}
				else { printf("ERROR: invalid graph optimizer flags: %s\n", argv[arg]); return -1; }
			}
			else { printf("ERROR: invalid option: %s\n", argv[arg]); return -1; }
		}
		else break;
	}
	if (arg == argc) { show_usage(program, false); return -1; }
	int argCount = argc - arg - 2;

	// get optional arguments
	int optionCount = 0;
	char optionString[1024] = { 0 };
	for (int i = 0; i < argCount; i++) {
		char * option = argv[arg + 2 + i];
		if (*option == '/') {
			optionCount++;
			char * p = strstr(option, "=");
			if (!strncmp(option, "/def-var:", 9) && p != NULL) {
				char * s = optionString + strlen(optionString);
				sprintf(s, "def-var %s\n", option + 9);
				*strstr(s, "=") = ' ';
			}
		}
		if (strstr(option, ":R,CAMERA-")) enableMultiFrameProcessing = true;
	}
	fflush(stdout);

	CVxEngine engine;
	int errorCode = 0;
	try {
		// initialize engine
		if (engine.Initialize(argCount - optionCount, defaultTargetAffinity, defaultTargetInfo, doUseProcessGraph, disableVirtual) < 0) throw -1;
		if (doSetGraphOptimizerFlags) {
			engine.SetGraphOptimizerFlags(graphOptimizerFlags);
		}
		engine.SetCaptureFrameStart(frameStart);
		fflush(stdout);
		for (int i = 0, j = 0; i < argCount; i++) {
			char * param = argv[arg + 2 + i];
			if (param[0] != '/') {
				// pass non-options as parameters to the engine
				if (engine.SetParameter(j++, param) < 0)
					throw -1;
			}
		}
		fflush(stdout);

		if (!strncmp(argv[arg], "file", 4)) {
			if ((arg+1) == argc){
				printf("ERROR: Need to specify a .gdf file to run\n");
				throw -1;
			}
			arg++;
			const char * fileName = RootDirUpdated(argv[arg]);
			CFileBuffer txt(fileName);
			char * txtBuffer = (char *)txt.GetBuffer();
			if (!txtBuffer) {
				printf("ERROR: unable to open: %s\n", fileName);
				throw -1;
			}
			char * fullText = new char[strlen(optionString) + strlen(txtBuffer) + 1];
			strcpy(fullText, optionString);
			strcat(fullText, txtBuffer);
			if (engine.BuildGraph(fullText, useAgoImport, useAgoDump) < 0)
				throw -1;
			delete[] fullText;
		}
		else if (!strncmp(argv[arg], "node", 4)) {
			char txt[1024];
			int paramCount = argc - arg - 2;
			arg++;
			sprintf(txt, "node %s ", argv[arg]);
			for (int i = 0, j = 0; i < paramCount; i++) {
				if (argv[arg + 1 + i][0] != '/'){
					sprintf(txt + strlen(txt), "$%d ", j++ + 1);
				}
			}
			if (engine.BuildGraph(txt, useAgoImport, useAgoDump) < 0) throw -1;
		}
		else { printf("ERROR: invalid command: %s\n", argv[arg]); throw -1; }
		fflush(stdout);
		if (doRun) {
			engine.SetVerbose(verbose);
			engine.SetAbortOnMismatch(abortOnMismatch);
			if (engine.IsUsingMultiFrameCapture()) {
				enableMultiFrameProcessing = true;
			}
			if (frameCountSpecified) {
				enableMultiFrameProcessing = false;
			}
			// execute the graph for all requested frames
			int count = 0, status = 0;
			printf("csv,HEADER ,STATUS, COUNT,cur-ms,avg-ms,min-ms,clenqueue-ms,clwait-ms,clwrite-ms,clread-ms\n");
			fflush(stdout);

			int64_t start_time = utilGetClockCounter();

			for (int frameNumber = frameStart; enableMultiFrameProcessing || frameNumber < frameEnd; frameNumber++, count++){
				//read input data, when specified
				if ((status = engine.ReadFrame(frameNumber)) < 0) throw -1;
				if (framesEofRequested && status > 0) {
					// data is not available
					if (frameNumber == frameStart) {
						ReportError("ERROR: insufficient input data -- check input files\n");
					}
					else break; 
				}
				// execute graph for current frame
				status = engine.ExecuteFrame(frameNumber);
				if (status == VX_ERROR_GRAPH_ABANDONED)
					break; // don't report graph abandoned as an error
				if (status < 0) throw -1;
				// compare output data, when requested
				status = engine.CompareFrame(frameNumber);
				// write output data, when requested
				if (engine.WriteFrame(frameNumber) < 0) throw -1;

				if (verbose) {
					printf("csv,FRAME  ,  %s,%s\n", (status == 0 ? "PASS" : "FAIL"), engine.MeasureFrame(frameNumber));
					fflush(stdout);
				}
				if (status < 0) throw - 1;
				else if (status) break;
				// display refresh
				if (ProcessCvWindowKeyRefresh() > 0)
					break;
			}
		
			int64_t end_time = utilGetClockCounter();
			int64_t frequency = utilGetClockFrequency();
			float elapsed_time = (float)(end_time - start_time) / frequency;
			printf("csv,OVERALL,  %s,%s\n", (status >= 0 ? "PASS" : "FAIL"), engine.GetPerformanceStatistics());
			printf("Elapsed Time: %6.2f sec\n", (float)elapsed_time);

			engine.GetMedianRunTime();
#if _DEBUG
			_CrtDumpMemoryLeaks();
#endif
			if (doGetInternalProfile) {
				char fileName[] = "stdout";
				engine.DumpInternalProfile(fileName);
			}
			fflush(stdout);
		}
		if (engine.Shutdown() < 0) throw -1;
		fflush(stdout);
	}
	catch (int errorCode_) {
		fflush(stdout);
		engine.DisableWaitForKeyPress();
		errorCode = errorCode_;
	}
	if (doPause) {
		fflush(stdout);
		printf("Press ENTER to exit ...\n");
		while (getchar() != '\n')
			;
	}
	return errorCode;
}
