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
#include "vxTensor.h"

///////////////////////////////////////////////////////////////////////
// class CVxParamTensor
//
CVxParamTensor::CVxParamTensor()
{
	// vx configuration
	m_vxObjType = VX_TYPE_TENSOR;
	m_num_of_dims = 0;
	for (vx_size i = 0; i < MAX_TENSOR_DIMENSIONS; i++)
		m_dims[i] = 1;
	m_data_type = VX_TYPE_INT16;
	m_fixed_point_pos = 0;
	// I/O configuration
	m_maxErrorLimit = 0;
	m_avgErrorLimit = 0;
	m_readFileIsBinary = false;
	m_writeFileIsBinary = false;
	m_compareFileIsBinary = false;
	m_compareCountMatches = 0;
	m_compareCountMismatches = 0;
	// vx object
	m_tensor = nullptr;
	m_vxObjRef = nullptr;
	m_data = nullptr;
	m_size = 0;
}

CVxParamTensor::~CVxParamTensor()
{
	Shutdown();
}

int CVxParamTensor::Shutdown(void)
{
	if (m_compareCountMatches > 0 && m_compareCountMismatches == 0) {
		printf("OK: tensor COMPARE MATCHED for %d frame(s) of %s\n", m_compareCountMatches, GetVxObjectName());
	}
	if (m_tensor) {
		vxReleaseTensor(&m_tensor);
		m_tensor = nullptr;
	}
	if (m_data) {
		delete[] m_data;
		m_data = nullptr;
	}
	return 0;
}

int CVxParamTensor::Initialize(vx_context context, vx_graph graph, const char * desc)
{
	// get object parameters and create object
	const char * ioParams = desc;
	if (!_strnicmp(desc, "tensor:", 7) || !_strnicmp(desc, "virtual-tensor:", 15)) {
		bool isVirtual = false;
		if (!_strnicmp(desc, "virtual-tensor:", 15)) {
			isVirtual = true;
			desc += 8;
		}
		char objType[64], data_type[64];
		ioParams = ScanParameters(desc, "tensor:<num-of-dims>,{dims},<data-type>,<fixed-point-pos>", "s:D,L,s,d", objType, &m_num_of_dims, &m_num_of_dims, m_dims, data_type, &m_fixed_point_pos);
		m_data_type = ovxName2Enum(data_type);
		if (isVirtual) {
			m_tensor = vxCreateVirtualTensor(graph, m_num_of_dims, m_dims, m_data_type, m_fixed_point_pos);
		}
		else {
			m_tensor = vxCreateTensor(context, m_num_of_dims, m_dims, m_data_type, m_fixed_point_pos);
		}
	}
	else if (!_strnicmp(desc, "tensor-from-roi:", 16)) {
		char objType[64], masterName[64];
		ioParams = ScanParameters(desc, "tensor-from-view:<tensor>,<view>", "s:s,D,L,L", objType, masterName, &m_num_of_dims, &m_num_of_dims, m_start, &m_num_of_dims, m_end);
		auto itMaster = m_paramMap->find(masterName);
		if (itMaster == m_paramMap->end())
			ReportError("ERROR: tensor [%s] doesn't exist for %s\n", masterName, desc);
		vx_tensor masterTensor = (vx_tensor)itMaster->second->GetVxObject();
		m_tensor = vxCreateTensorFromROI(masterTensor, m_num_of_dims, m_start, m_end);
	}
	else ReportError("ERROR: unsupported tensor type: %s\n", desc);
	vx_status ovxStatus = vxGetStatus((vx_reference)m_tensor);
	if (ovxStatus != VX_SUCCESS){
		printf("ERROR: tensor creation failed => %d (%s)\n", ovxStatus, ovxEnum2Name(ovxStatus));
		if (m_tensor) vxReleaseTensor(&m_tensor);
		throw - 1;
	}
	m_vxObjRef = (vx_reference)m_tensor;

	// io initialize
	return InitializeIO(context, graph, m_vxObjRef, ioParams);
}

int CVxParamTensor::InitializeIO(vx_context context, vx_graph graph, vx_reference ref, const char * io_params)
{
	// save reference object and get object attributes
	m_vxObjRef = ref;
	m_tensor = (vx_tensor)m_vxObjRef;
	ERROR_CHECK(vxQueryTensor(m_tensor, VX_TENSOR_NUM_OF_DIMS, &m_num_of_dims, sizeof(m_num_of_dims)));
	ERROR_CHECK(vxQueryTensor(m_tensor, VX_TENSOR_DIMS, &m_dims, sizeof(m_dims[0])*m_num_of_dims));
	ERROR_CHECK(vxQueryTensor(m_tensor, VX_TENSOR_DATA_TYPE, &m_data_type, sizeof(m_data_type)));
	ERROR_CHECK(vxQueryTensor(m_tensor, VX_TENSOR_FIXED_POINT_POS, &m_fixed_point_pos, sizeof(vx_uint8)));
	m_size = m_data_type == VX_TYPE_FLOAT32 ? 4 : 2;
	for (vx_uint32 i = 0; i < m_num_of_dims; i++) {
		m_stride[i] = m_size;
		m_size *= m_dims[i];
	}
	m_data = new vx_uint8[m_size];
	if (!m_data) ReportError("ERROR: memory allocation failed for tensor: %u\n", (vx_uint32)m_size);

	// process I/O parameters
	if (*io_params == ':') io_params++;
	while (*io_params) {
		char ioType[64], fileName[256];
		io_params = ScanParameters(io_params, "<io-operation>,<parameter>", "s,S", ioType, fileName);
		if (!_stricmp(ioType, "read"))
		{ // read request syntax: read,<fileName>[,ascii|binary]
			m_fileNameRead.assign(RootDirUpdated(fileName));
			m_fileNameForReadHasIndex = (m_fileNameRead.find("%") != m_fileNameRead.npos) ? true : false;
			m_readFileIsBinary = true;
			while (*io_params == ',') {
				char option[64];
				io_params = ScanParameters(io_params, ",binary", ",s", option);
				if (!_stricmp(option, "binary")) {
					m_readFileIsBinary = true;
				}
				else ReportError("ERROR: invalid tensor read option: %s\n", option);
			}
		}
		else if (!_stricmp(ioType, "write"))
		{ // write request syntax: write,<fileName>[,ascii|binary]
			m_fileNameWrite.assign(RootDirUpdated(fileName));
			m_writeFileIsBinary = true;
			while (*io_params == ',') {
				char option[64];
				io_params = ScanParameters(io_params, ",binary", ",s", option);
				if (!_stricmp(option, "binary")) {
					m_writeFileIsBinary = true;
				}
				else ReportError("ERROR: invalid tensor write option: %s\n", option);
			}
		}
		else if (!_stricmp(ioType, "compare"))
		{ // write request syntax: compare,<fileName>[,ascii|binary]
			m_fileNameCompare.assign(RootDirUpdated(fileName));
			m_compareFileIsBinary = true;
			while (*io_params == ',') {
				char option[64];
				io_params = ScanParameters(io_params, ",binary|maxerr=<value>|avgerr=<value>", ",s", option);
				if (!_stricmp(option, "binary")) {
					m_compareFileIsBinary = true;
				}
				else if (!_strnicmp(option, "maxerr=", 7)) {
					m_maxErrorLimit = (float)atof(&option[7]);
					m_avgErrorLimit = 1e20f;
				}
				else if (!_strnicmp(option, "avgerr=", 7)) {
					m_avgErrorLimit = (float)atof(&option[7]);
					m_maxErrorLimit = 1e20f;
				}
				else ReportError("ERROR: invalid tensor compare option: %s\n", option);
			}
		}
		else if (!_stricmp(ioType, "directive") && (!_stricmp(fileName, "VX_DIRECTIVE_AMD_COPY_TO_OPENCL") || !_stricmp(fileName, "sync-cl-write"))) {
			m_useSyncOpenCLWriteDirective = true;
		}
		else ReportError("ERROR: invalid tensor operation: %s\n", ioType);
		if (*io_params == ':') io_params++;
		else if (*io_params) ReportError("ERROR: unexpected character sequence in parameter specification: %s\n", io_params);
	}

	return 0;
}

int CVxParamTensor::Finalize()
{
	// process user requested directives
	if (m_useSyncOpenCLWriteDirective) {
		ERROR_CHECK_AND_WARN(vxDirective((vx_reference)m_tensor, VX_DIRECTIVE_AMD_COPY_TO_OPENCL), VX_ERROR_NOT_ALLOCATED);
	}

	return 0;
}

int CVxParamTensor::ReadFrame(int frameNumber)
{
	// check if there is no user request to read
	if (m_fileNameRead.length() < 1) return 0;

	// for single frame reads, there is no need to read the array again
	// as it is already read into the object
	if (!m_fileNameForReadHasIndex && frameNumber != m_captureFrameStart) {
		return 0;
	}

	// reading data from input file
	char fileName[MAX_FILE_NAME_LENGTH]; sprintf(fileName, m_fileNameRead.c_str(), frameNumber);
	FILE * fp = fopen(fileName, m_readFileIsBinary ? "rb" : "r");
	if (!fp) {
		if (frameNumber == m_captureFrameStart) {
			ReportError("ERROR: Unable to open: %s\n", fileName);
		}
		else {
			return 1; // end of sequence detected for multiframe sequences
		}
	}
	if (fread(m_data, 1, m_size, fp) != m_size)
		ReportError("ERROR: not enough data (%d bytes) in %s\n", (vx_uint32)m_size, fileName);
	vx_status status = vxCopyTensorPatch(m_tensor, m_num_of_dims, nullptr, nullptr, m_stride, m_data, VX_WRITE_ONLY, VX_MEMORY_TYPE_HOST);
	fclose(fp);
	if (status != VX_SUCCESS)
		ReportError("ERROR: vxCopyTensorPatch: write failed (%d)\n", status);

	// process user requested directives
	if (m_useSyncOpenCLWriteDirective) {
		ERROR_CHECK_AND_WARN(vxDirective((vx_reference)m_tensor, VX_DIRECTIVE_AMD_COPY_TO_OPENCL), VX_ERROR_NOT_ALLOCATED);
	}

	return 0;
}

int CVxParamTensor::WriteFrame(int frameNumber)
{
	// check if there is no user request to write
	if (m_fileNameWrite.length() < 1) return 0;
	// read data from tensor
	vx_status status = vxCopyTensorPatch(m_tensor, m_num_of_dims, nullptr, nullptr, m_stride, m_data, VX_READ_ONLY, VX_MEMORY_TYPE_HOST);
	if (status != VX_SUCCESS)
		ReportError("ERROR: vxCopyTensorPatch: read failed (%d)\n", status);
	// write data to output file
	char fileName[MAX_FILE_NAME_LENGTH]; sprintf(fileName, m_fileNameWrite.c_str(), frameNumber);
	FILE * fp = fopen(fileName, m_writeFileIsBinary ? "wb" : "w");
	if (!fp) ReportError("ERROR: Unable to create: %s\n", fileName);
	fwrite(m_data, 1, m_size, fp);
	fclose(fp);

	return 0;
}

int CVxParamTensor::CompareFrame(int frameNumber)
{
	// check if there is no user request to compare
	if (m_fileNameCompare.length() < 1) return 0;

	// reading data from reference file
	char fileName[MAX_FILE_NAME_LENGTH]; sprintf(fileName, m_fileNameCompare.c_str(), frameNumber);
	FILE * fp = fopen(fileName, m_compareFileIsBinary ? "rb" : "r");
	if (!fp) {
		ReportError("ERROR: Unable to open: %s\n", fileName);
	}
	if (fread(m_data, 1, m_size, fp) != m_size)
		ReportError("ERROR: not enough data (%d bytes) in %s\n", (vx_uint32)m_size, fileName);
	fclose(fp);

	// compare
	vx_map_id map_id;
	vx_size stride[MAX_TENSOR_DIMENSIONS];
	vx_uint8 * ptr;
	vx_status status = vxMapTensorPatch(m_tensor, m_num_of_dims, nullptr, nullptr, &map_id, stride, (void **)&ptr, VX_READ_ONLY, VX_MEMORY_TYPE_HOST, 0);
	if (status != VX_SUCCESS)
		ReportError("ERROR: vxMapTensorPatch: read failed (%d)\n", status);

	bool mismatchDetected = false;
	if (m_data_type == VX_TYPE_INT16) {
		vx_int32 maxError = 0;
		vx_int64 sumError = 0;
		for (vx_size d3 = 0; d3 < m_dims[3]; d3++) {
			for (vx_size d2 = 0; d2 < m_dims[2]; d2++) {
				for (vx_size d1 = 0; d1 < m_dims[1]; d1++) {
					vx_size roffset = m_stride[3] * d3 + m_stride[2] * d2 + m_stride[1] * d1;
					vx_size doffset = stride[3] * d3 + stride[2] * d2 + stride[1] * d1;
					const vx_int16 * buf1 = (const vx_int16 *)(((vx_uint8 *)ptr) + doffset);
					const vx_int16 * buf2 = (const vx_int16 *)(m_data + roffset);
					for (vx_size d0 = 0; d0 < m_dims[0]; d0++) {
						vx_int32 v1 = buf1[d0];
						vx_int32 v2 = buf2[d0];
						vx_int32 d = v1 - v2;
						d = (d < 0) ? -d : d;
						maxError = (d > maxError) ? d : maxError;
						sumError += d * d;
					}
				}
			}
		}
		vx_size count = m_dims[0] * m_dims[1] * m_dims[2] * m_dims[3];
		float avgError = (float)sumError / (float)count;
		mismatchDetected = ((float)maxError > m_maxErrorLimit) ? true : false;
		mismatchDetected = ((float)avgError > m_avgErrorLimit) ? true : mismatchDetected;
		if (mismatchDetected)
			printf("ERROR: tensor COMPARE MISMATCHED [max-err: %d] [avg-err: %.6f] for %s with frame#%d of %s\n", maxError, avgError, GetVxObjectName(), frameNumber, fileName);
		else if (m_verbose)
			printf("OK: tensor COMPARE MATCHED [max-err: %d] [avg-err: %.6f] for %s with frame#%d of %s\n", maxError, avgError, GetVxObjectName(), frameNumber, fileName);
	}
	else if (m_data_type == VX_TYPE_FLOAT32) {
		vx_float32 maxError = 0;
		vx_float64 sumError = 0;
		for (vx_size d3 = 0; d3 < m_dims[3]; d3++) {
			for (vx_size d2 = 0; d2 < m_dims[2]; d2++) {
				for (vx_size d1 = 0; d1 < m_dims[1]; d1++) {
					vx_size roffset = m_stride[3] * d3 + m_stride[2] * d2 + m_stride[1] * d1;
					vx_size doffset = stride[3] * d3 + stride[2] * d2 + stride[1] * d1;
					const vx_float32 * buf1 = (const vx_float32 *)(((vx_uint8 *)ptr) + doffset);
					const vx_float32 * buf2 = (const vx_float32 *)(m_data + roffset);
					for (vx_size d0 = 0; d0 < m_dims[0]; d0++) {
						vx_float32 v1 = buf1[d0];
						vx_float32 v2 = buf2[d0];
						vx_float32 d = v1 - v2;
						d = (d < 0) ? -d : d;
						maxError = (d > maxError) ? d : maxError;
						sumError += d * d;
					}
				}
			}
		}
		vx_size count = m_dims[0] * m_dims[1] * m_dims[2] * m_dims[3];
		float avgError = (float)sumError / (float)count;
		mismatchDetected = (maxError > m_maxErrorLimit) ? true : false;
		mismatchDetected = (avgError > m_avgErrorLimit) ? true : mismatchDetected;
		if (mismatchDetected)
			printf("ERROR: tensor COMPARE MISMATCHED [max-err: %.6f] [avg-err: %.6f] for %s with frame#%d of %s\n", maxError, avgError, GetVxObjectName(), frameNumber, fileName);
		else if (m_verbose)
			printf("OK: tensor COMPARE MATCHED [max-err: %.6f] [avg-err: %.6f] for %s with frame#%d of %s\n", maxError, avgError, GetVxObjectName(), frameNumber, fileName);
	}
	else if (m_data_type == VX_TYPE_FLOAT16) {
		vx_float32 maxError = 0;
		vx_float64 sumError = 0;
		for (vx_size d3 = 0; d3 < m_dims[3]; d3++) {
			for (vx_size d2 = 0; d2 < m_dims[2]; d2++) {
				for (vx_size d1 = 0; d1 < m_dims[1]; d1++) {
					vx_size roffset = m_stride[3] * d3 + m_stride[2] * d2 + m_stride[1] * d1;
					vx_size doffset = stride[3] * d3 + stride[2] * d2 + stride[1] * d1;
					const vx_uint16 * buf1 = (const vx_uint16 *)(((vx_uint8 *)ptr) + doffset);
					const vx_uint16 * buf2 = (const vx_uint16 *)(m_data + roffset);
					for (vx_size d0 = 0; d0 < m_dims[0]; d0++) {
						vx_uint16 h1 = buf1[d0];
						vx_uint16 h2 = buf2[d0];
						vx_uint32 d1 = ((h1 & 0x8000) << 16) | (((h1 & 0x7c00) + 0x1c000) << 13) | ((h1 & 0x03ff) << 13);
						vx_uint32 d2 = ((h2 & 0x8000) << 16) | (((h2 & 0x7c00) + 0x1c000) << 13) | ((h2 & 0x03ff) << 13);
						vx_float32 v1 = *(float *)&d1;
						vx_float32 v2 = *(float *)&d2;
						vx_float32 d = v1 - v2;
						d = (d < 0) ? -d : d;
						maxError = (d > maxError) ? d : maxError;
						sumError += d * d;
					}
				}
			}
		}
		vx_size count = m_dims[0] * m_dims[1] * m_dims[2] * m_dims[3];
		float avgError = (float)sumError / (float)count;
		mismatchDetected = (maxError > m_maxErrorLimit) ? true : false;
		mismatchDetected = (avgError > m_avgErrorLimit) ? true : mismatchDetected;
		if (mismatchDetected)
			printf("ERROR: tensor COMPARE MISMATCHED [max-err: %.6f] [avg-err: %.6f] for %s with frame#%d of %s\n", maxError, avgError, GetVxObjectName(), frameNumber, fileName);
		else if (m_verbose)
			printf("OK: tensor COMPARE MATCHED [max-err: %.6f] [avg-err: %.6f] for %s with frame#%d of %s\n", maxError, avgError, GetVxObjectName(), frameNumber, fileName);
	}
	else {
		for (vx_size d3 = 0; d3 < m_dims[3]; d3++) {
			for (vx_size d2 = 0; d2 < m_dims[2]; d2++) {
				for (vx_size d1 = 0; d1 < m_dims[1]; d1++) {
					vx_size roffset = m_stride[3] * d3 + m_stride[2] * d2 + m_stride[1] * d1;
					vx_size doffset = stride[3] * d3 + stride[2] * d2 + stride[1] * d1;
					if (memcpy(((vx_uint8 *)ptr) + doffset, m_data + roffset, stride[0] * m_dims[0])) {
						mismatchDetected = true;
						break;
					}
				}
				if (mismatchDetected)
					break;
			}
			if (mismatchDetected)
				break;
		}
		if (mismatchDetected)
			printf("ERROR: tensor COMPARE MISMATCHED for %s with frame#%d of %s\n", GetVxObjectName(), frameNumber, fileName);
		else if (m_verbose) 
			printf("OK: tensor COMPARE MATCHED for %s with frame#%d of %s\n", GetVxObjectName(), frameNumber, fileName);
	}

	status = vxUnmapTensorPatch(m_tensor, map_id);
	if (status != VX_SUCCESS)
		ReportError("ERROR: vxUnmapTensorPatch: read failed (%d)\n", status);

	// report error if mismatched
	if (mismatchDetected) {
		m_compareCountMismatches++;
		if (!m_discardCompareErrors) return -1;
	}
	else {
		m_compareCountMatches++;
	}

	return 0;
}
