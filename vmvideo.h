/****************************************

Файл vmvideo.h

Заголовочный файл модуля vmvideo.c
Кузьминых Кирилл	10.05.2016

****************************************/

#ifndef _VMVIDEO_H_
#define _VMVIDEO_H_


//----------------------------------------

typedef struct _vmvideo_struct vmvideo_struct;

//----------------------------------------

vmvideo_struct * create_vmvideo (unsigned int width, unsigned int height);
void vmvideo_write_videomem (vmvideo_struct *vmvideo, unsigned int addr, unsigned int count, unsigned char *srcBuf);

//----------------------------------------

#endif  // _VMVIDEO_H_
