# AMD RunVX USER GUIDE

## DESCRIPTION
This guide provides an overview of the content and usage of the RunVX tool. This tool is used to run OpenVX graphs, with a simple, easy-to-use interface. It encapsulates most of the routine OpenVX calls, thus speeding up development and enabling rapid prototyping.

## GETTING STARTED
The RunVX tool is simple to setup and use. The graph is provided as a GDF format file (graph description file), a simple and intuitive syntax to describe the various nodes and the dependencies. The tool has other useful features like comparing outputs, visualizing output keypoints on the picture window, using the OpenCV library.

### Supported Development Environments
The following are the minimum requirements for running the AMD RunVX tool.
* OS:             Microsoft Windows 7/8.1/10 (64-bit only), Ubuntu 15.10 64-bit 
* IDE/Compiler:   Microsoft Visual Studio Professional 2013 (for example code)
* CPU:            SSE4.1 or above CPU, 64-bit.
* GPU:            Radeon R7 Series or above (Kaveri+ APU), Radeon 3xx Series or above
* DRIVER:         AMD Catalyst 15.7 (version 15.20) with OpenCL2.0 runtimes
* SUPPORT TOOLS
    * AMD APP SDK v3.0 Beta or higher (64-bit) from [here](http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/)
    * OpenCV 3.0 compliant cameras and webcams are required for graphs using camera inputs. The following cameras have been tested with this version
        * Microsoft LifeCam HD3000
        * Logitech c270 HD Webcam
        * Creative Senz3D VF0780 
        * HP laptop built-in camera

## RUNVX TOOL
RunVX is a command line tool used to execute OpenVX graphs. As input, RUNVX takes a GDF (Graph Description Format) file and outputs images, pyramids, arrays, distributions, etc. This guide describes the elements of RunVX tool and the syntax used to run OpenVX graphs.

### RunVX Syntax
    runvx.exe [options] file <GDF-file> argument(s)
    runvx.exe [options] node <kernelName> argument(s)

    options:
      -h                                 -- show full help
      -v                                 -- verbose
      -root:<directory>                  -- replace ~ in filenames with directory 
                                              in the command-line and GDF file. 
                                              The default value of '~' is '.' 
                                              when this option is not specified.
      -frames:[<start>[,<end>]]/[live]   -- run the graph/node for specified frames
      -frames-or-eof::[<start>[,<end>]]  -- run the graph/node for specified frames
                                              or until eof
      -affinity:<type>                   -- set default target affinity to type 
                                              (shall be CPU or GPU)
      -ago-profile                       -- print node-level performance data for 
                                              profiling
      -no-abort-on-mismatch              -- continue graph processing even if a 
                                              mismatch occurs
      -no-run                            -- abort after vxVerifyGraph
      -disable-virtual                   -- replace all virtual data types in GDF 
                                              with non-virtual data types
    
    kernelName:
    The supported kernel name list is given below.
        org.khronos.openvx.color_convert
        org.khronos.openvx.channel_extract
        org.khronos.openvx.channel_combine
        org.khronos.openvx.sobel_3x3
        org.khronos.openvx.magnitude
        org.khronos.openvx.phase
        org.khronos.openvx.scale_image
        org.khronos.openvx.table_lookup
        org.khronos.openvx.histogram
        org.khronos.openvx.equalize_histogram
        org.khronos.openvx.absdiff
        org.khronos.openvx.mean_stddev
        org.khronos.openvx.threshold
        org.khronos.openvx.integral_image
        org.khronos.openvx.dilate_3x3
        org.khronos.openvx.erode_3x3
        org.khronos.openvx.median_3x3
        org.khronos.openvx.box_3x3
        org.khronos.openvx.gaussian_3x3
        org.khronos.openvx.custom_convolution
        org.khronos.openvx.gaussian_pyramid
        org.khronos.openvx.accumulate
        org.khronos.openvx.accumulate_weighted
        org.khronos.openvx.accumulate_square
        org.khronos.openvx.minmaxloc
        org.khronos.openvx.convertdepth
        org.khronos.openvx.canny_edge_detector
        org.khronos.openvx.and
        org.khronos.openvx.or
        org.khronos.openvx.xor
        org.khronos.openvx.not
        org.khronos.openvx.multiply
        org.khronos.openvx.add
        org.khronos.openvx.subtract
        org.khronos.openvx.warp_affine
        org.khronos.openvx.warp_perspective
        org.khronos.openvx.harris_corners
        org.khronos.openvx.fast_corners
        org.khronos.openvx.optical_flow_pyr_lk
        org.khronos.openvx.remap
        org.khronos.openvx.halfscale_gaussian
        

    argument(s):
    GDF objects can be created on command-line with I/O capability and passed into a GDF file. A GDF file can access these GDF objects using $1, $2, $3, etc. corresponding to its position command-line. In addition to read/write/compare/initialize, image objects can be connected to a camera, image and keypoint-array objects can be displayed in a window. See below for argument specification, which is an extension to GDF object syntax for limited set of GDF objects.

    image:<width>,<height>,<format>[[:<io-params>]...]
    image-virtual:<width>,<height>,<image-format>
    image-uniform:<width>,<height>,<image-format>,<uniform-pixel-value>[[:<io-params>]...] 
      <image-format>         -   four letter string with values of VX_DF_IMAGE (see vx_df_image_e in vx_types.h)
                                 some useful formats are: RGB2,RGBX,IYUV,NV12,U008,S016,U032,U001,F032,...
      <uniform-pixel-val>    -   pixel value for uniform image
      <io-params>            -   READ,<fileNameorURL>[,frames{<start>[;<count>;repeat]}] or camera,deviceNumber
                                   to read input from file, URL, camera 
                                 VIEW,<window-name> to display in a window
                                 WRITE,<fileNameOrURL> to write
                                 COMPARE,<fileName>[,rect{<start-x>;<start-y>;<end-x>;<end-y>}][,err{<min>;<max>}]
                                   [,checksum|checksum-save-instead-of-test]
                                   to compare output for exact-match or md5 checksum match or match
                                   with range of pixel values, or generate md5 checksum without comparing.

    array:<format>,<capacity>[[:<io-params>]...]
    array-virtual:<format>,<capacity>[[:<io-params>]...]
      <format>               -   KEYPOINT/COORDINATES2D/COORDINATES3D/RECTANGLE
      <io-params>            -   READ,<fileName>[,ascii|binary] to read 
                                 VIEW,<window-name> to display points overlaid in a window
                                 WRITE,<fileName>[,ascii|binary] to write
                                 COMPARE,<fileName>[,ascii|binary][,err{<x>;<y>[;<strength>]}]

    pyramid:<numLevels>,half|orb|<scale-factor>,<width>,<height>,<image-format>[:<io-params>]
    pyramid-virtual:<numLevels>,half|orb|<scale-factor>,<width>,<height>,<image-format>
      <numLevels>            -   number of levels of the pyramid
      <image-format>         -   four letter string with values of VX_DF_IMAGE (see vx_df_image_e in vx_types.h)
                                   some useful formats are: U008,S016,...
    <io-params>              -   READ,<fileName[%n]> read image into pyramid at level n
                                 WRITE,<fileName[%n]> write level n to file
                                 COMPARE,<fileName>[,rect{<start-x>;<start-y>;<end-x>;<end-y>}][,err{<min>;<max>}]
                                 [,checksum|checksum-save-instead-of-test]
                                   to compare output for exact-match or md5 checksum match or match
                                   with range of pixel values, or generate md5 checksum without comparing

    distribution:<numBins>,<offset>,<range>[[:<io-params>]...]
      <numBins>              -   num of bins
      <offset>               -   start offset value
      <range>                -   end value to use as range
      <io-params>            -   READ,<fileName>[,ascii|binary] to read 
                                 VIEW,<window-name> to overlay distribution visualization on a window
                                 WRITE,<fileName>[,ascii|binary] to write
                                 COMPARE,<fileName>[,ascii|binary] exact match compare

    convolution:<columns>,<rows>[[:<io-params>]...]
      <io-params>            -   READ,<fileName>[,ascii|binary|scale] to read 
                                 WRITE,<fileName>[,ascii|binary|scale] to write
                                 COMPARE,<fileName>[,ascii|binary|scale] exact match compare
                                 SCALE,<scale-factor> scale value of the convolution
                                 INIT,{<value1>;<value2>;...<valueN>} initialize matrix to immediate value

    lut:<data-type>,<count>[[:<io-params>]...]
      <data-type>            -   useful data types are: UINT8
      <io-params>            -   READ,<fileName>[,ascii|binary] to read 
                                 WRITE,<fileName>[,ascii|binary] to write
                                 COMPARE,<fileName>[,ascii|binary] exact match compare
                                 VIEW,<window-name> to view lookup table on a window

    matrix:<data-type>,<columns>,<rows>[:<mode>,<fileName>]
      <data-type>            -   useful data types are: INT32, FLOAT32
      <io-params>            -   READ,<fileName>[,ascii|binary] to read 
                                 WRITE,<fileName>[,ascii|binary] to write
                                 COMPARE,<fileName>[,ascii|binary] exact match compare
                                 INIT,{<value1>;<value2>;...<valueN>} initialize matrix to immediate value

    remap:<srcWidth>,<srcHeight>,<dstWidth>,<dstHeight>[:<io-params>]
      <io-params>            -   READ,<fileName>[,ascii|binary] to read 
                                 WRITE,<fileName>[,ascii|binary] to write
                                 COMPARE,<fileName>[,ascii|binary][,err{<x>;<y>}] compare within range
                                 INIT,<filename>|rotate-90|rotate-180|rotate-270|scale|hflip|vflip
                                   initialize remap table with file or in-built remaps

    scalar:<data-type>,<value>[:<io-params>]
      <data-type>            -   any scalar datatype supported by OpenVX some useful data types are: INT32, UINT32, FLOAT32, ENUM, BOOL, SIZE, ...
      <io-params>            -   READ,<fileName> to read from file
                                 WRITE,<fileName> to write to file
                                 COMPARE,<fileName>[,range] compare within range in file,separated by space
                                 VIEW,<window-name> to view scalar value on a window

    threshold:<thresh-type>,<data-type>[:<io-params>]
      <thresh-type>          -   BINARY/RANGE
      <data-type>            -   useful data types are: UINT8, INT16
      <io-params>            -   READ,<fileName> to read from file
                                 INIT,<value1>[,<value2>] threshold value or range

## Examples
Here are few examples that demonstrate use of RUNVX prototyping tool.

### Canny Edge Detector
This example demonstrates building OpenVX graph for Canny edge detector. Use [raja-koduri-640x480.jpg](http://cdn5.applesencia.com/wp-content/blogs.dir/17/files/2013/04/raja-koduri-640x480.jpg) for this example.

    % runvx[.exe] file canny.gdf image:640,480,RGB2:READ,raja-koduri-640x480.jpg:VIEW,RGB image:640,480,U008:VIEW,SKIN

File **canny.gdf**:

    # compute luma image channel from input RGB image
    data yuv  = image-virtual:0,0,IYUV
    data luma = image-virtual:0,0,U008
    node org.khronos.openvx.color_convert $1 yuv
    node org.khronos.openvx.channel_extract yuv !CHANNEL_Y luma

    # compute edges in luma image using Canny edge detector
    data hyst = threshold:RANGE,UINT8:INIT,80,100
    data gradient_size = scalar:INT32,3
    node org.khronos.openvx.canny_edge_detector luma hyst gradient_size !NORM_L1 $2
    
### Skintone Pixel Detector
This example demonstrates building OpenVX graph for pixel-based skin tone detector [Peer et al. 2003]. Use [raja-koduri-640x480.jpg](http://cdn5.applesencia.com/wp-content/blogs.dir/17/files/2013/04/raja-koduri-640x480.jpg) for this example.

    % runvx[.exe] file skintonedetect.gdf image:640,480,RGB2:READ,raja-koduri-640x480.jpg:VIEW,RGB image:640,480,U008:VIEW,SKIN

File **skintonedetect.gdf**:

    # threshold objects
    data thr95  = threshold:BINARY,UINT8:INIT,95 # threshold for computing R > 95
    data thr40  = threshold:BINARY,UINT8:INIT,40 # threshold for computing G > 40
    data thr20  = threshold:BINARY,UINT8:INIT,20 # threshold for computing B > 20
    data thr15  = threshold:BINARY,UINT8:INIT,15 # threshold for computing R-G > 15
    data thr0   = threshold:BINARY,UINT8:INIT,0  # threshold for computing R-B > 0

    # virtual image objects for intermediate results
    data R      = image-virtual:0,0,U008
    data G      = image-virtual:0,0,U008
    data B      = image-virtual:0,0,U008
    data RmG    = image-virtual:0,0,U008
    data RmB    = image-virtual:0,0,U008
    data R95    = image-virtual:0,0,U008
    data G40    = image-virtual:0,0,U008
    data B20    = image-virtual:0,0,U008
    data RmG15  = image-virtual:0,0,U008
    data RmB0   = image-virtual:0,0,U008
    data and1   = image-virtual:0,0,U008
    data and2   = image-virtual:0,0,U008
    data and3   = image-virtual:0,0,U008

    # extract R,G,B channels and compute R-G and R-B
    node org.khronos.openvx.channel_extract $1 !CHANNEL_B R # extract R channel from input (argument 1)
    node org.khronos.openvx.channel_extract $1 !CHANNEL_G G # extract G channel from input (argument 1)
    node org.khronos.openvx.channel_extract $1 !CHANNEL_R B # extract B channel from input (argument 1)
    node org.khronos.openvx.subtract R   G   !SATURATE RmG  # compute R-G
    node org.khronos.openvx.subtract R   B   !SATURATE RmB  # compute R-B

    # compute threshold
    node org.khronos.openvx.threshold R   thr95 R95         # compute R > 95
    node org.khronos.openvx.threshold G   thr40 G40         # compute G > 40
    node org.khronos.openvx.threshold B   thr20 B20         # compute B > 20
    node org.khronos.openvx.threshold RmG thr15 RmG15       # compute RmG > 15
    node org.khronos.openvx.threshold RmB thr0  RmB0        # compute RmB > 0

    # aggregate all thresholded values to produce SKIN pixels
    node org.khronos.openvx.and R95   G40   and1            # compute R95 & G40
    node org.khronos.openvx.and and1  B20   and2            # compute B20 & and1
    node org.khronos.openvx.and RmG15 RmB0  and3            # compute RmG15 & RmB0
    node org.khronos.openvx.and and2 and3 $2                # compute and2 & and3 as output (argument 2)
    
