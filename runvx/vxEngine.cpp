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
#include "vxEngine.h"

#define DEBUG_INFO 0
#define DEBUG_GRAPH 0

vector<string> &splittwo(const string &s, char delim, vector<string> &elems){
	if (delim == ' ') {
		const char * p = s.c_str();
		while (*p) {
			while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
				p++;
			const char * q = p;
			while (*p && !(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
				p++;
			if (*q){
				char item[1024];
				strncpy(item, q, p - q); item[p - q] = 0;
				elems.push_back(item);
			}
		}
	}
	else {
		stringstream ss(s);
		string item;
		while (getline(ss, item, delim)){
			elems.push_back(item);
		}
	}
	return elems;
}

void RemoveVirtualKeywordFromParamDescription(std::string& paramDesc)
{
	size_t pos = paramDesc.find("-virtual:");
	if (pos != std::string::npos) {
		paramDesc.erase(pos, 8);
	}
}

static void VX_CALLBACK log_callback(vx_context context, vx_reference ref, vx_status status, const vx_char string[])
{
	printf("%s", string); fflush(stdout);
}

CVxEngine::CVxEngine()
{
	m_paramCount = 0;
	int64_t freq = utilGetClockFrequency();
	m_perfMultiplicationFactor = 1000.0f / freq; // to convert clock counter to ms
	m_usingMultiFrameCapture = false;
	m_captureFrameStart = 0;
	m_disableVirtual = false;
	m_verbose = false;
}

CVxEngine::~CVxEngine()
{
	Shutdown();
}

int CVxEngine::Initialize(int argCount, int defaultTargetAffinity, int defaultTargetInfo, bool useProcessGraph, bool disableVirtual)
{
	// save configuration
	m_paramCount = argCount;
	m_useProcessGraph = useProcessGraph;
	vx_status ovxStatus = VX_SUCCESS;
	m_disableVirtual = disableVirtual;

	// create OpenVX context, register log_callback, and show implementation
	m_context = vxCreateContext();
	if (vxGetStatus((vx_reference)m_context)) { printf("ERROR: vxCreateContext failed\n"); throw - 1; }
	vxRegisterLogCallback(m_context, log_callback, vx_false_e);
	char name[VX_MAX_IMPLEMENTATION_NAME];
	if (ovxStatus = vxQueryContext(m_context, VX_CONTEXT_ATTRIBUTE_IMPLEMENTATION, name, VX_MAX_IMPLEMENTATION_NAME)) {
		printf("ERROR: vxQueryContext)VX_CONTEXT_ATTRIBUTE_IMPLEMENTATION) failed (%d:%s)\n", ovxStatus, ovxEnum2Name(ovxStatus));
		return -1;
	}
	printf("OK: using %s\n", name);

	// set default target affinity, if request
	if (defaultTargetAffinity) {
		AgoTargetAffinityInfo attr_affinity = { 0 };
		attr_affinity.device_type = defaultTargetAffinity;
		attr_affinity.device_info = defaultTargetInfo;
		vx_status status = vxSetContextAttribute(m_context, VX_CONTEXT_ATTRIBUTE_AMD_AFFINITY, &attr_affinity, sizeof(attr_affinity));
		if (status) {
			printf("ERROR: vxSetContextAttribute(VX_CONTEXT_ATTRIBUTE_AMD_AFFINITY,%d) failed (%d:%s)\n", defaultTargetAffinity, status, ovxEnum2Name(status));
			throw - 1;
		}
	}

	// create graph
	m_graph = vxCreateGraph(m_context);
	if (vxGetStatus((vx_reference)m_graph)){ printf("ERROR: vxCreateGraph failed\n"); throw - 1; }

	return 0;
}

int CVxEngine::SetGraphOptimizerFlags(vx_uint32 graph_optimizer_flags)
{
	// set optimizer flags
	vx_status status = vxSetGraphAttribute(m_graph, VX_GRAPH_ATTRIBUTE_AMD_OPTIMIZER_FLAGS, &graph_optimizer_flags, sizeof(graph_optimizer_flags));
	if (status) {
		printf("ERROR: vxSetGraphAttribute(*,VX_GRAPH_ATTRIBUTE_AMD_OPTIMIZER_FLAGS,%d) failed (%d:%s)\n", graph_optimizer_flags, status, ovxEnum2Name(status));
		throw - 1;
	}
	return 0;
}

vx_context CVxEngine::getContext()
{
	return m_context;
}

int CVxEngine::SetParameter(int index, const char * param)
{
	std::string paramDesc;
	if (m_disableVirtual) {
		paramDesc = param;
		RemoveVirtualKeywordFromParamDescription(paramDesc);
		param = paramDesc.c_str();
	}
	CVxParameter * parameter = CreateDataObject(m_context, m_graph, &m_paramMap, &m_userStructMap, param, m_captureFrameStart);
	if (!parameter) {
		printf("ERROR: unable to create parameter# %d\nCheck syntax: %s\n", index, param);
		return -1;
	}

	vx_reference ref;
	char name[16];
	sprintf(name, "$%d", index+1);
	m_paramMap.insert(pair<string, CVxParameter *>(name, parameter));
	ref = m_paramMap[name]->GetVxObject();
	vxSetReferenceName(ref, name);
	return 0;
}

int CVxEngine::SetImportedData(vx_reference ref, const char * name, const char * params)
{
	if (params) {
		CVxParameter * obj = CreateDataObject(m_context, m_graph, ref, params, m_captureFrameStart);
		if (!obj) {
			printf("ERROR: CreateDataObject(*,*,*,%s) failed\n", params);
			return -1;
		}
		m_paramMap.insert(pair<string, CVxParameter *>(name, obj));
		vxSetReferenceName(ref, name);
	}
	return 0;
}

void VX_CALLBACK CVxEngine_data_registry_callback_f(void * obj, vx_reference ref, const char * name, const char * params)
{
	int status = ((CVxEngine *)obj)->SetImportedData(ref, name, params);
	if (status) {
		printf("ERROR: SetImportedData(*,%s,%s) failed (%d)\n", name, params, status);
		throw -1;
	}
}

int CVxEngine::BuildGraph(char * graphScript, bool useAgoImport, bool useAgoDump)
{
	if (useAgoImport) {
		vx_reference ref[64] = { 0 };
		int num_ref = 0;
		for (int i = 0; i < 64; i++) {
			char name[16]; sprintf(name, "$%d", i + 1);
			if (m_paramMap.find(name) == m_paramMap.end())
				break;
			ref[num_ref++] = m_paramMap[name]->GetVxObject();
		}
		// parse the graph using import
		AgoGraphImportInfo info = { 0 };
		info.text = graphScript;
		info.ref = ref;
		info.num_ref = num_ref;
		info.data_registry_callback_obj = this;
		info.data_registry_callback_f = CVxEngine_data_registry_callback_f;
		if (useAgoDump) info.dumpToConsole = 2;
		vx_status status = vxSetGraphAttribute(m_graph, VX_GRAPH_ATTRIBUTE_AMD_IMPORT_FROM_TEXT, &info, sizeof(info));
		if (status != VX_SUCCESS) {
			printf("ERROR: vxSetGraphAttribute(...,VX_GRAPH_ATTRIBUTE_AMD_IMPORT_FROM_TEXT,...) failed (%d)\n", status);
			return -1;
		}
	}
	else {
		// parse the graph text
		char currentLine[2048];
		memset(currentLine, 0, 2048);
		string currentLine_string = "";
		bool is_comment = false;

		vector<string> graphScript_vector;
		int j = 0;
		for (size_t i = 0; i < strlen(graphScript); i++){
			if (graphScript[i] != '\r' && graphScript[i] != '\n'){
				if (graphScript[i] == '#'){
					is_comment = true;
				}
				if (is_comment == false){
					currentLine[j] = graphScript[i];
					j++;
				}
			}
			else if (graphScript[i] == '\n'){
					currentLine[j] = '\0';
					graphScript_vector.push_back(currentLine);
					j = 0;
				is_comment = false;
			}
		}
			currentLine[j] = '\0';
			graphScript_vector.push_back(currentLine);
			memset(currentLine, 0, 2048);
#if DEBUG_GRAPH
		printf("\n\nDEBUG Graph script here:\n");
		for (vector<string>::size_type i = 0; i != graphScript_vector.size(); i++){
			printf("%d: %s\n", i, graphScript_vector[i].c_str());
		}
#endif
		vector<string> graphLine_vector;
		vx_status status = 0;

		//user defined structs
		vx_size currentStructSize = 0;
		vx_enum currentStructEnum = 0;

		for (vector<string>::size_type i = 0; i != graphScript_vector.size(); ++i){
			splittwo(graphScript_vector[i].c_str(), ' ', graphLine_vector);
			if (graphLine_vector.size() > 0){							//crashes if graphLine_vector.size() == 0
				if (strcmp(graphLine_vector[0].c_str(), "import") == 0){
					if (graphLine_vector.size() != 2){
						printf("ERROR: SYNTAX, check line with \'import %s\'\n", graphLine_vector[1].c_str());
						return -1;
					}
					status |= vxLoadKernels(m_context, graphLine_vector[1].c_str());
					if (status) {
						printf("ERROR: vxLoadKernels(context, %s) failed(%d)\n", graphLine_vector[1].c_str(), status);
						return -1;
					}
				}
				else if (strcmp(graphLine_vector[0].c_str(), "type") == 0){
					if (graphLine_vector.size() != 3){
						printf("ERROR: SYNTAX, check line with \'type %s\'\n", graphLine_vector[1].c_str());
						printf("       SYNTAX for register-user-struct creation is \'type [OBJECT NAME] [OBJECT TYPE]:[SIZE IN BYTES]'\n");
						return -1;
					}
					vector<string> typeObject;
					splittwo(graphLine_vector[2].c_str(), ':', typeObject);
					if (strcmp(typeObject[0].c_str(), "userstruct") == 0){
						istringstream(typeObject[1]) >> currentStructSize;
						currentStructEnum = vxRegisterUserStruct(m_context, currentStructSize);
						if (currentStructEnum == VX_TYPE_INVALID){
							printf("ERROR: Could not create register-user-struct %s, returned VX_TYPE_INVALID. Exiting\n", graphLine_vector[1].c_str());
						}
						m_userStructMap.insert(pair<string, vx_enum>(graphLine_vector[1].c_str(), currentStructEnum));
					}
					else{
						printf("ERROR: OBJECT TYPE of %s is not supported. Exiting.\n", typeObject[0].c_str());
						return -1;
					}
				}
				else if (strcmp(graphLine_vector[0].c_str(), "data") == 0){
					if (graphLine_vector.size() != 4){
						printf("ERROR: SYNTAX, check line with \'data %s\'\n", graphLine_vector[1].c_str());
						return -1;
					}
					if ((strcmp(graphLine_vector[2].c_str(), "=") == 0)){
						std::string paramDesc = graphLine_vector[3];
						if (m_disableVirtual) {
							RemoveVirtualKeywordFromParamDescription(paramDesc);
						}
						CVxParameter * obj = CreateDataObject(m_context, m_graph, &m_paramMap, &m_userStructMap, paramDesc.c_str(), m_captureFrameStart);
						if (!obj) {
							printf("ERROR: SYNTAX, \'%s\' is an INVALID object\n", graphLine_vector[3].c_str());
							return -1;
						}
						m_paramMap.insert(pair<string, CVxParameter *>(graphLine_vector[1].c_str(), obj));
						vx_reference ref = m_paramMap[graphLine_vector[1].c_str()]->GetVxObject();
						vx_status status = VX_SUCCESS;
						status = vxSetReferenceName(ref, graphLine_vector[1].c_str());
						if (status != VX_SUCCESS){
							printf("ERROR: vxSetReferenceName returned %d\n", status);
							return -1;
						}
					}
					else {
						printf("ERROR: SYNTAX for data object creation is \'data [DATA OBJECT NAME] = [DATA OBJECT PARAMETERS]\'\n");
						return -1;
					}
				}
				else if (strcmp(graphLine_vector[0].c_str(), "node") == 0){
					if (graphLine_vector.size() >= 3){
						//initialize kernel
						char thisNode[2048];
						sprintf(thisNode, "%s", graphLine_vector[1].c_str());
						vx_kernel current_kernel = vxGetKernelByName(m_context, thisNode);
						if (current_kernel == 0) { printf("ERROR: vxGetKernelByName(%s) failed\n", thisNode); throw -1; }
						vx_node current_node = vxCreateGenericNode(m_graph, current_kernel);
						if (current_node == 0) { printf("ERROR: vxCreateGenericNode(%s) failed\n", thisNode); throw -1; }
						int paramNum = 0;
						int skipParam = 0;
						for (vector<string>::size_type j = 2; j != graphLine_vector.size(); ++j){
							skipParam = 0;
							if (!strncmp(graphLine_vector[j].c_str(), "!", 1)){
								//printf("DEBUG: ENUM, %s\n", graphLine_vector[j]);
								string enum_name_s;
								size_t length_of_enum = graphLine_vector[j].size() - 1;
								char description[2048];
								char desc2[2048];

								enum_name_s = graphLine_vector[j].substr(1, length_of_enum);
#if DEBUG_INFO
								printf("DEBUG: enum_name = %s\n", enum_name_s.c_str());
#endif				
								strcpy(description, "scalar:enum,%s");
								sprintf(desc2, description, enum_name_s.c_str());
								if (m_disableVirtual) {
									std::string paramDesc = desc2;
									RemoveVirtualKeywordFromParamDescription(paramDesc);
									strcpy(desc2, paramDesc.c_str());
								}
								CVxParameter * obj = CreateDataObject(m_context, m_graph, &m_paramMap, &m_userStructMap, desc2, m_captureFrameStart);
								if (!obj) {
									printf("ERROR: CreateDataObject(*,*,%s) failed\n", desc2);
									return -1;
								}
								m_paramMap.insert(pair<string, CVxParameter *>(graphLine_vector[j].c_str(), obj));
							}
							else if (!strncmp(graphLine_vector[j].c_str(), "attr:BORDER_MODE:CONSTANT,", 26)){
								vx_border_mode_t this_border_mode = { 0 };
								vector<string> attributeValue_vector;
								splittwo(graphLine_vector[j].c_str(), ',', attributeValue_vector);
								istringstream(attributeValue_vector[1].c_str()) >> this_border_mode.constant_value;
								this_border_mode.mode = VX_BORDER_MODE_CONSTANT;
								vxSetNodeAttribute(current_node, VX_NODE_ATTRIBUTE_BORDER_MODE, &this_border_mode, sizeof(this_border_mode));
								skipParam = 1;
							}
							else if (!strncmp(graphLine_vector[j].c_str(), "attr:BORDER_MODE:UNDEFINED", 26)){
								vx_border_mode_t this_border_mode = { 0 };
								vector<string> attributeValue_vector;
								splittwo(graphLine_vector[j].c_str(), ',', attributeValue_vector);
								this_border_mode.mode = VX_BORDER_MODE_UNDEFINED;
								vxSetNodeAttribute(current_node, VX_NODE_ATTRIBUTE_BORDER_MODE, &this_border_mode, sizeof(this_border_mode));
								skipParam = 1;
							}
							else if (!strncmp(graphLine_vector[j].c_str(), "attr:BORDER_MODE:REPLICATE", 26)){
								vx_border_mode_t this_border_mode = { 0 };
								vector<string> attributeValue_vector;
								splittwo(graphLine_vector[j].c_str(), ',', attributeValue_vector);
								this_border_mode.mode = VX_BORDER_MODE_REPLICATE;
								vxSetNodeAttribute(current_node, VX_NODE_ATTRIBUTE_BORDER_MODE, &this_border_mode, sizeof(this_border_mode));
								skipParam = 1;
							}
							else if (!strncmp(graphLine_vector[j].c_str(), "attr:affinity:CPU", 26)) {
							}
							else if (!strncmp(graphLine_vector[j].c_str(), "attr:affinity:GPU", 26)) {
							}
							if (skipParam == 0){
#if DEBUG_INFO
								printf("Node: %s, graphLine_vector[%d] =  %s\n", thisNode, j, graphLine_vector[j].c_str());
#endif				
								if (strcmp(graphLine_vector[j].c_str(), "null") != 0) {
									if (m_paramMap.find(graphLine_vector[j].c_str()) == m_paramMap.end()) {
										printf("ERROR: BuildGraph: parameter named %s is not found\n", graphLine_vector[j].c_str());
										return -1;
									}
									else {
										status = vxSetParameterByIndex(current_node, paramNum, m_paramMap[graphLine_vector[j].c_str()]->GetVxObject());
										if (status) {
											printf("ERROR: BuildGraph: vxSetParameterByIndex(...,%d,%s,...) failed (%d)\n", paramNum, graphLine_vector[j].c_str(), status);
											return -1;
										}
									}
								}
								paramNum++;
							}
						}
						memset(thisNode, 0, 2048);
					}
					else{
						printf("ERROR: SYNTAX for any node is \'node [NAME OF NODE] [NODE ARGUMENT(S)]\n");
						return -1;
					}
				}
				else if (graphLine_vector[0] == "affinity") {
					if (graphLine_vector.size() != 2 || (graphLine_vector[1] != "CPU" && graphLine_vector[1] != "GPU")) {
						printf("ERROR: SYNTAX, check line with \'affinity %s\' (use CPU/GPU)\n", graphLine_vector[1].c_str());
						return -1;
					}
					vx_uint32 defaultTargetAffinity = (graphLine_vector[1] == "GPU") ? AGO_TARGET_AFFINITY_GPU : AGO_TARGET_AFFINITY_CPU;
					AgoTargetAffinityInfo attr_affinity = { 0 };
					attr_affinity.device_type = defaultTargetAffinity;
					status = vxSetGraphAttribute(m_graph, VX_GRAPH_ATTRIBUTE_AMD_AFFINITY, &attr_affinity, sizeof(attr_affinity));
					if (status) {
						printf("ERROR: vxSetContextAttribute(%d) failed (%d)\n", defaultTargetAffinity, status);
						throw - 1;
					}
				}
				else {
					printf("ERROR: SYNTAX, \'%s\' is not recognized as a keyword\n", graphLine_vector[0].c_str());
					return -1;
				}
			}
			graphLine_vector.clear();
		}
		graphScript_vector.clear();
	}

	// dump original graph (if requested)
	if (useAgoDump) {
		vx_reference ref[64] = { 0 };
		int num_ref = 0;
		for (int i = 0; i < 64; i++) {
			char name[16]; sprintf(name, "$%d", i + 1);
			if (m_paramMap.find(name) == m_paramMap.end())
				break;
			ref[num_ref++] = m_paramMap[name]->GetVxObject();
		}
		AgoGraphExportInfo info = { 0 };
		strcpy(info.fileName, "stdout");
		info.ref = ref;
		info.num_ref = num_ref;
		strcpy(info.comment, "original");
		vx_status status = vxSetGraphAttribute(m_graph, VX_GRAPH_ATTRIBUTE_AMD_EXPORT_TO_TEXT, &info, sizeof(info));
		if (status != VX_SUCCESS) {
			printf("ERROR: vxSetGraphAttribute(...,VX_GRAPH_ATTRIBUTE_AMD_EXPORT_TO_TEXT,...) failed (%d)\n", status);
			return -1;
		}
	}

	// verify the graph
	vx_status status;
	if ((status = vxVerifyGraph(m_graph)) != VX_SUCCESS){
		printf("ERROR: vxVerifyGraph failed. status = %d\n", status);
		return -1;
	}

	// dump optimized graph (if requested)
	if (useAgoDump) {
		vx_reference ref[64] = { 0 };
		int num_ref = 0;
		for (int i = 0; i < 64; i++) {
			char name[16]; sprintf(name, "$%d", i + 1);
			if (m_paramMap.find(name) == m_paramMap.end())
				break;
			ref[num_ref++] = m_paramMap[name]->GetVxObject();
		}
		AgoGraphExportInfo info = { 0 };
		strcpy(info.fileName, "stdout");
		info.ref = ref;
		info.num_ref = num_ref;
		strcpy(info.comment, "drama");
		vx_status status = vxSetGraphAttribute(m_graph, VX_GRAPH_ATTRIBUTE_AMD_EXPORT_TO_TEXT, &info, sizeof(info));
		if (status != VX_SUCCESS) {
			printf("ERROR: vxSetGraphAttribute(...,VX_GRAPH_ATTRIBUTE_AMD_EXPORT_TO_TEXT,...) failed (%d)\n", status);
			return -1;
		}
		fflush(stdout);
	}

	// Finalize() on all objects in graph
	for (auto it = m_paramMap.begin(); it != m_paramMap.end(); ++it){
		if (m_usingMultiFrameCapture == false){
			m_usingMultiFrameCapture = it->second->IsUsingMultiFrameCapture();
		}
		it->second->Finalize();
	}

#if DEBUG_INFO
	for (auto it = m_paramMap.begin(); it != m_paramMap.end(); ++it){
		printf("Parameter name: %s\n", it->first);
	}
	fflush(stdout);
#endif

	return 0;
}

int CVxEngine::ExecuteFrame(int frameNumber)
{
	if (!m_useProcessGraph) {
		vx_status graph_status;

		if ((graph_status = vxScheduleGraph(m_graph)) != VX_SUCCESS){
			if (graph_status == VX_ERROR_GRAPH_ABANDONED) {
				printf("WARNING: graph execution abandoned by a kernel or application\n");
			}
			else printf("ERROR: vxScheduleGraph failed, status = %d\n", graph_status);
			return graph_status;
	    }

		if ((graph_status = vxWaitGraph(m_graph)) != VX_SUCCESS) {
			if (graph_status == VX_ERROR_GRAPH_ABANDONED) {
				printf("WARNING: graph execution abandoned by a kernel or application\n");
			}
			else printf("ERROR: vxWaitGraph failed.\n");
			return graph_status;
		}
	}
	else {
		vx_status status = vxProcessGraph(m_graph);
		if (status) {
			if (status == VX_ERROR_GRAPH_ABANDONED) {
				printf("WARNING: graph execution abandoned by a kernel or application\n");
			}
			else printf("ERROR: vxProcessGraph() => %d\n", status);
			return status;
		}
	}
	
	return 0;
}

int CVxEngine::ReadFrame(int frameNumber)
{
	for (auto it = m_paramMap.begin(); it != m_paramMap.end(); ++it){
		int status = it->second->ReadFrame(frameNumber);
		if (status)
			return status;
	}
	return 0;
}

int CVxEngine::WriteFrame(int frameNumber)
{
	
	for (auto it = m_paramMap.begin(); it != m_paramMap.end(); ++it){
		int status = it->second->WriteFrame(frameNumber);
		if (status)
			return status;
	}
	return 0;
}

int CVxEngine::CompareFrame(int frameNumber)
{
	for (auto it = m_paramMap.begin(); it != m_paramMap.end(); ++it){
		int status = it->second->CompareFrame(frameNumber);
		if (status)
			return status;
	}
	return 0;
}

void CVxEngine::SetVerbose(bool verbose)
{
	m_verbose = verbose;
	if (verbose) {
		for (auto it = m_paramMap.begin(); it != m_paramMap.end(); ++it){
			it->second->SetVerbose(verbose);
		}
	}
}

void CVxEngine::SetAbortOnMismatch(bool abortOnMismatch)
{
	for (auto it = m_paramMap.begin(); it != m_paramMap.end(); ++it){
		it->second->SetAbortOnMismatch(abortOnMismatch);
	}
}

const char * CVxEngine::MeasureFrame(int frameNumber)
{
	vx_perf_t perf = { 0 };
	float current_time = 0.0;
	vx_status status = vxQueryGraph(m_graph, VX_GRAPH_ATTRIBUTE_PERFORMANCE, &perf, sizeof(perf));
	if (status) {
		printf("ERROR: vxQueryGraph(*,VX_GRAPH_ATTRIBUTE_PERFORMANCE,...) failed (%d)\n", status);
		throw -1;
	}
	static char text[256];
	sprintf(text, "%6d,%6.2f,%6.2f,%6.2f", (int)perf.num,
		(float)perf.tmp * m_perfMultiplicationFactor,
		(float)perf.avg * m_perfMultiplicationFactor,
		(float)perf.min * m_perfMultiplicationFactor);

	current_time = (float)perf.tmp * m_perfMultiplicationFactor;
	m_times_vector.push_back(current_time);

	AgoGraphPerfInternalInfo iperf = { 0 };
	status = vxQueryGraph(m_graph, VX_GRAPH_ATTRIBUTE_AMD_PERFORMANCE_INTERNAL_LAST, &iperf, sizeof(iperf));
	if (status) { printf("ERROR: vxQueryGraph(*,VX_GRAPH_ATTRIBUTE_AMD_PERFORMANCE_INTERNAL_LAST,...) failed (%d)\n", status); throw - 1; }
	sprintf(text + strlen(text), ",%6.2f,%6.2f,%6.2f,%6.2f",
		(float)iperf.kernel_enqueue * m_perfMultiplicationFactor,
		(float)iperf.kernel_wait * m_perfMultiplicationFactor,
		(float)iperf.buffer_write * m_perfMultiplicationFactor,
		(float)iperf.buffer_read * m_perfMultiplicationFactor);

	return text;
}


void CVxEngine::GetMedianRunTime()
{
	if (m_times_vector.size() > 2){
		sort(m_times_vector.begin(), m_times_vector.end());
		int mod_2 = m_times_vector.size() % 2;
		float median = 0.0;
		double middle_index = floor(m_times_vector.size() / 2);
		int middle_index_i = (int)middle_index;
		if (mod_2 == 0){

			median = (m_times_vector[middle_index_i] + m_times_vector[middle_index_i - 1]) / 2;
		}
		else {
			median = m_times_vector[(int)middle_index_i];
		}
		printf("Median = %.3f\n", median);
	}
}


const char * CVxEngine::GetPerformanceStatistics()
{
	vx_perf_t perf = { 0 };
	vx_status status = vxQueryGraph(m_graph, VX_GRAPH_ATTRIBUTE_PERFORMANCE, &perf, sizeof(perf));
	if (status) {
		printf("ERROR: vxQueryGraph(*,VX_GRAPH_ATTRIBUTE_PERFORMANCE,...) failed (%d)\n", status);
		throw -1;
	}
	static char text[256];
	sprintf(text, "%6d,      ,%6.2f,%6.2f", (int)perf.num,
		(float)perf.avg * m_perfMultiplicationFactor,
		(float)perf.min * m_perfMultiplicationFactor);

	AgoGraphPerfInternalInfo iperf = { 0 };
	status = vxQueryGraph(m_graph, VX_GRAPH_ATTRIBUTE_AMD_PERFORMANCE_INTERNAL_AVG, &iperf, sizeof(iperf));
	if (status) { printf("ERROR: vxQueryGraph(*,VX_GRAPH_ATTRIBUTE_AMD_PERFORMANCE_INTERNAL_AVG,...) failed (%d)\n", status); throw - 1; }
	sprintf(text + strlen(text), ",%6.2f,%6.2f,%6.2f,%6.2f",
		(float)iperf.kernel_enqueue * m_perfMultiplicationFactor,
		(float)iperf.kernel_wait * m_perfMultiplicationFactor,
		(float)iperf.buffer_write * m_perfMultiplicationFactor,
		(float)iperf.buffer_read * m_perfMultiplicationFactor);

	return text;
}

vx_status CVxEngine::DumpInternalProfile(char * fileName)
{
	return vxQueryGraph(m_graph, VX_GRAPH_ATTRIBUTE_AMD_PERFORMANCE_INTERNAL_PROFILE, fileName, 0);
}

int CVxEngine::Shutdown()
{
	for (auto it = m_paramMap.begin(); it != m_paramMap.end(); ++it){
		if (it->second){
			delete it->second;
		}
	}
	m_paramMap.clear();
	if (m_graph){
		vxReleaseGraph(&m_graph);
		m_graph = nullptr;
	}


	if (m_context) {
		vxReleaseContext(&m_context);
		m_context = nullptr;
	}
	return 0;
}

void CVxEngine::DisableWaitForKeyPress()
{
	for (auto it = m_paramMap.begin(); it != m_paramMap.end(); ++it){
		if (it->second){
			it->second->DisableWaitForKeyPress();
		}
	}
}

bool CVxEngine::IsUsingMultiFrameCapture(){
	return m_usingMultiFrameCapture;
}
