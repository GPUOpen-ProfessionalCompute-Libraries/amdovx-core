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


#ifndef CVX_ENGINE_H
#define CVX_ENGINE_H

#include "vxParameter.h"

class CVxEngine {

public:
	CVxEngine();
	virtual ~CVxEngine();
	// functions to setup
	// returns 0 on SUCCESS, else error code
	int Initialize(int paramCount, int defaultTargetAffinity, int defaultTargetInfo, bool useProcessGraph, bool disableVirtual);
	int SetGraphOptimizerFlags(vx_uint32 graph_optimizer_flags);
	vx_context getContext();
	int SetParameter(int index, const char * param);
	void viewParameters();
	void GetMedianRunTime();
	int BuildGraph(char * graphScript, bool useAgoImport, bool useAgoDump);
	// functions for execution
	// returns 0 on SUCCESS, else error code
	// ReadFrame() returns +ve value to indicate data unavailability
	int ReadFrame(int frameNumber);
	int ExecuteFrame(int frameNumber);
	int WriteFrame(int frameNumber);
	int CompareFrame(int frameNumber);
	const char * MeasureFrame(int frameNumber);
	// functions for profiling
	vx_status DumpInternalProfile(char * fileName);
	const char * GetPerformanceStatistics();
	// functions for termination
	int Shutdown();
	void DisableWaitForKeyPress();
	// save data object and parameters
	int SetImportedData(vx_reference ref, const char * name, const char * params);
	bool IsUsingMultiFrameCapture();
	void SetCaptureFrameStart(vx_uint32 frameStart) { m_captureFrameStart = frameStart; }
	void SetVerbose(bool verbose);
	void SetAbortOnMismatch(bool abortOnMismatch);

private:
	// implementation specific functions
	// ExecuteVXU - run utility that correspond to m_vxuName - called by ExecuteFrame
	int ExecuteVXU();

private:
	// implementation specific data
	// m_paramMap - holds names and pointers to all data objects
	// m_paramCount - number of data objects on command-line
	// m_vxuName - contains vxuName when BuildUtility() is called, otherwise nullptr
	map<string, CVxParameter *> m_paramMap;
	map<string, vx_enum> m_userStructMap;
	int m_paramCount;
	const char * m_vxuName;
	vx_context m_context;
	vx_graph m_graph;
	float m_perfMultiplicationFactor; // to convert vx_perf_t to milli-seconds
	bool m_useProcessGraph;
	vector<float> m_times_vector;
	// to support multi-frame capture
	bool m_usingMultiFrameCapture;
	vx_uint32 m_captureFrameStart;
	// disable virtual objects
	bool m_disableVirtual;
	// verbose flag
	bool m_verbose;
};
#endif /* CVX_ENGINE_H*/