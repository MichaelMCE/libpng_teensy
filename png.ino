// Simple test of USB Host
//
// This example is in the public domain


#include "usbhost/USBHost_t36.h"
#include "usbd480/usbd480.h"
#include "SdFat.h"

#include "pngcommon.h"


static USBHost myusb;
static USBHub hub1(myusb);
static USBD480Display usbd480(myusb);
static SdFatSdioEX sdEx;
static File file;

int doTests = 0;




bool sdInit ()
{
	if (!sdEx.begin()){
      Serial.println("sdEx.begin() failed");
      return false;
	}

	sdEx.chvol();
	return 1;
}

int readPngToFrame (uint8_t *frame, png_structp *png_ptr, int bpp, int width, int height, int ox, int oy, int passCount)
{
	uint8_t line[(width * 4) + 4];		// enough for a single RGBA line

	int ret = 1;
	switch (bpp){
	  case 8:  readPng32To8      (frame, png_ptr, width, height, line, ox, oy, passCount); break;
	  case 12: readPng32To12     (frame, png_ptr, width, height, line, ox, oy, passCount); break; // non packed 12bit
	  case 15: readPng32To16_555 (frame, png_ptr, width, height, line, ox, oy, passCount); break;
	  case 16: readPng32To16_565 (frame, png_ptr, width, height, line, ox, oy, passCount); break;
	  case 24: readPng32To24     (frame, png_ptr, width, height, line, ox, oy, passCount); break;
	  case 32: readPng32To32A_cpy(frame, png_ptr, width, height, line, ox, oy, passCount); break;
	  default: Serial.printf("libpng: format %i not implemented\r\n", bpp); ret = 0;
	};

	return ret;
}

#if 0
void PNGAPI warningCallback (png_structp png_ptr, png_const_charp warning_msg)
{
	char *err = (char *)png_get_error_ptr(png_ptr);
	if (err)
		Serial.printf("libpng: warning (%s), %s\r\n",err, (intptr_t)warning_msg);
	else
		Serial.printf("libpng: warning %s\r\n",(intptr_t)warning_msg);
	
}
#endif

void png_read_dataCb (png_structp png_ptr, png_bytep data, png_size_t length)
{

	//Serial.printf("png_read_dataCb %p %i \r\n", data, length);
	//File *file = (File*)png_ptr->io_ptr;

	png_size_t check = (png_size_t)file.read(data, length);
	if (check != length)
		Serial.printf("read Error, length:%i\r\n", length);
}

int32_t loadPng (uint8_t *frame, const int flags, const char *filename, int32_t ox, int32_t oy, uint32_t *width, uint32_t *height, uint32_t *bpp)
{
	png_structp png_ptr;
	png_infop info_ptr;
	unsigned int sig_read = 0;

	
	if (!file.open(filename, O_RDONLY)){
		return 0;
	}
	
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		file.close();
		return 0;
	}

	png_set_crc_action(png_ptr, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
#if 0
	png_set_error_fn(png_ptr, (png_voidp) NULL, (png_error_ptr) warningCallback, warningCallback);
#endif
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		file.close();
		return 0;
	}

   if (setjmp(png_jmpbuf(png_ptr))){
      // Free all of the memory associated with the png_ptr and info_ptr 
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	  file.close();
      // if we get here, we had a problem reading the file
      return 0;
   }

	//png_init_io(png_ptr, fp); 
	png_set_read_fn(png_ptr, (png_voidp)&file, png_read_dataCb);
	 
	png_set_sig_bytes(png_ptr, sig_read);
	png_read_info(png_ptr, info_ptr);

	int passCount = 1;
	int bit_depth = 0, color_type = 0, interlace_type = 0;
	png_get_IHDR(png_ptr, info_ptr, (png_uint_32*)width, (png_uint_32*)height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
	if (bpp) *bpp = bit_depth;
	
	if (frame != NULL){
		if (interlace_type == PNG_INTERLACE_ADAM7)
			passCount = png_set_interlace_handling(png_ptr);
		//printf("interlace_type %i, ct:%i, %i %i\n", interlace_type, passCount, *width, *height);

	
		if (bit_depth == 16)
			png_set_strip_16(png_ptr);
		png_set_packing(png_ptr);

		if (color_type == PNG_COLOR_TYPE_PALETTE)
			png_set_palette_to_rgb(png_ptr);
		else if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
			png_set_expand_gray_1_2_4_to_8(png_ptr);

		if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
			png_set_tRNS_to_alpha(png_ptr);
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
		
		/*if (flags&LOAD_RESIZE){
			if (!_resizeFrame(frame, *width, *height, 0))
				return 0;
		}*/
	}

	int ret = 1;
	if (frame)
		ret = readPngToFrame(frame, &png_ptr, flags, *width, *height, ox, oy, passCount);
	
	if (!ret)
		png_read_end(png_ptr, info_ptr);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	
	file.close();
	return ret;
}

int32_t png_read (const char *filename, uint8_t *buffer, const int bufferbpp, int ox, int oy)
{
	uint32_t width = 0;
	uint32_t height = 0;
	
	return loadPng(buffer, bufferbpp, filename, ox, oy, &width, &height, NULL);
}

int32_t png_metrics (const char *filename, uint32_t *width, uint32_t *height, uint32_t *filebpp)
{
	*width = 0;
	*height = 0;
	*filebpp = 0;

	return loadPng(NULL, 0, filename, 0, 0, width, height, filebpp);
}

int usbd_callback (uint32_t msg, intptr_t *value1, uint32_t value2)
{
	//Serial.printf("usbd_callback %i %i %i", (int)msg, value1, value2);
	//Serial.println();
	
	if (msg == USBD_MSG_DISPLAYREADY){
		delay(25);
		doTests = 1;
	}
	
	return 1;
}

void setup ()
{
	while (!Serial) ; // wait for Arduino Serial Monitor
	
	Serial.println("libpng testing");
	
	myusb.begin();
	usbd480.setCallbackFunc(usbd_callback);
	delay(25);
	sdInit();
}

void pngTest (const char *filename, const int des_x, const int des_y)
{
	uint32_t width, height, bpc;	// bpc:bits per channel

	if (png_metrics(filename, &width, &height, &bpc)){
		//Serial.printf("width %i, height %i, bpp %i\r\n", width, height, bpc);
				
		uint8_t buffer[width * height * sizeof(uint16_t)];
		if (png_read(filename, buffer, BPP_16, 0, 0))		// 16bpp - destination [buffer] bits per pixel
			usbd480.drawScreenArea(buffer, width, height, des_x, des_y);
	}
}

static const char *images[] = {
	"somuchwin24.png",
	"somuchwin24.png",
	"somuchwin24.png",
	"somuchwin24.png",
	"7.png",
	"8.png",
	"dvb.png",
	"exit.png",
	NULL
};


static const uint16_t colours[] = {
	RGB_16_WHITE,
	RGB_16_RED,
	RGB_16_GREEN,
	RGB_16_BLUE
};

void loop ()
{

	myusb.Task();
	
	if (doTests){
		doTests = 0;
		
		delay(100);
		
		int cx = (usbd480.display.width - 130)/2;		// 130: few of the images are 130x130 (intended for old nokia displays)
		int cy = (usbd480.display.height - 130)/2;
		
		for (int i = 0; i < 100; i++){
			usbd480.fillScreen(colours[i&0x03]); 
			
			pngTest(images[0], cx, cy);
			pngTest(images[1], 0, 0);
			pngTest(images[2], 100, 100);
			pngTest(images[3], usbd480.display.width-140, usbd480.display.height-140);
			pngTest(images[4], cx, 10);
			pngTest(images[5], 30, cy);
			pngTest(images[6], 0, 0);
			pngTest(images[7], usbd480.display.width-140, 0);
		}
	}
}

