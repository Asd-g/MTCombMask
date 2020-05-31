# Description

This filter produces a mask showing areas that are combed.

The thresholds work as following: after calculating the combing value, if one is below thY1, the pixel is set to 0, if above thY2, it is set to 255, and if inbetween, it is set to the combing value divided by 256.

This is the same filter as the one from MaskTools-v1.5.8 with few changes:
- chroma could be filtered;
- Y/YUV(A) 8..16-bit clips are accepted;
- support for v8 interface;
- removed MMX asm code;
- registered as MT_NICE_FILTER.

# Usage

```
CombMask(clip, int "thY1", int "thY2", int "y", int "u", int "v", bool "usemmx")
```

## Parameters:

- clip\
    A clip to process. It must have constant format and it must be Y/YUV(A) 8..16-bit.
    
- thY1\
    Pixels below thY1 are set to 0.\
    Must be between 0 and 255 and must be <= thY2.\
    Default: 30.
    
- thY2\
    Pixels above thY2 are set to 255.\
    Must be between 0 and 255 and must be >= thY1.\
    Default: 30.
    
- y, u , v\
    Select which planes to process.\
    Must be between 1 and 3.\
    1: No process the plane.\
    2: Copy the plane.\
    3: Process the plane.\
    Default: y = 3, u = 1, v = 1.
    
- usemmx\
    It's a dummy parameter for backward compatibility.\
    Default: True.
