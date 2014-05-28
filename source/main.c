/* 
	Application Controller PS3 main.c
	
	main.c based on sprite2D TINY3D sample / (c) 2010 Hermes  <www.elotrolado.net>
*/

#include <ppu-lv2.h>

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <sysutil/video.h>
#include <time.h>

#include <net/net.h>
#include <net/netctl.h>
#include <net/socket.h>
#include <errno.h>

#include <zlib.h>
#include <io/pad.h>

#include <sysmodule/sysmodule.h>
#include <jpgdec/jpgdec.h>

#include <tiny3d.h>
#include <libfont.h>

#include <ppu-lv2.h>

#include "internet_jpg.h"
#include "font.h"


u32 * texture_mem, * jpg_texture_mem;		// Pointers to texture memory and where the jpg texture should be allocated
jpgData texture_jpg;						// Contains image printed on screen
u32 texture_jpg_offset; 					// offset for texture (used to pass the texture)

int screen_width = 0, screen_height = 0;	// Set to dimensions of the screen in main()

float fps = 0;								// Float containing the current calculated FPS
char showFPS = 0;							// Boolean whether to print FPS on screen

int originalSocket = 0;						// First socket, startServer() returns newsockfd as connection between PS3 and PC, not this
int count_frames = 0;						// Frame count, used for FPS

//Port number to bind
#define portno 3000

//States for the pad packet
#define ANA_Left_Up 1
#define ANA_Right_Down 2

//header ID's. Set in the first byte of the header
#define requestPad 1
#define updateImage 2
#define updateImageZLIB 3
#define updateShowFPS 4
#define exitToXMB 5
#define resetToListen 6

/*
	From sprite2D source
	I'm not going to document them for that reason
*/

// draw one background color in virtual 2D coordinates
void DrawBackground2D(u32 rgba)
{
    tiny3d_SetPolygon(TINY3D_QUADS);

    tiny3d_VertexPos(0  , 0  , 65535);
    tiny3d_VertexColor(rgba);

    tiny3d_VertexPos(847, 0  , 65535);

    tiny3d_VertexPos(847, 511, 65535);

    tiny3d_VertexPos(0  , 511, 65535);
    tiny3d_End();
}

void DrawSprites2D(float x, float y, float layer, float dx, float dy, u32 rgba)
{
    tiny3d_SetPolygon(TINY3D_QUADS);

    tiny3d_VertexPos(x     , y     , layer);
    tiny3d_VertexColor(rgba);
    tiny3d_VertexTexture(0.0f, 0.0f);

    tiny3d_VertexPos(x + dx, y     , layer);
    tiny3d_VertexTexture(0.99f, 0.0f);

    tiny3d_VertexPos(x + dx, y + dy, layer);
    tiny3d_VertexTexture(0.99f, 0.99f);

    tiny3d_VertexPos(x     , y + dy, layer);
    tiny3d_VertexTexture(0.0f, 0.99f);

    tiny3d_End();
}

void DrawSpritesRot2D(float x, float y, float layer, float dx, float dy, u32 rgba, float angle)
{
    dx /= 2.0f; dy /= 2.0f;

    MATRIX matrix;
    
    // rotate and translate the sprite
    matrix = MatrixRotationZ(angle);
    matrix = MatrixMultiply(matrix, MatrixTranslation(x + dx, y + dy, 0.0f));
    
    // fix ModelView Matrix
    tiny3d_SetMatrixModelView(&matrix);
   
    tiny3d_SetPolygon(TINY3D_QUADS);

    tiny3d_VertexPos(-dx, -dy, layer);
    tiny3d_VertexColor(rgba);
    tiny3d_VertexTexture(0.0f , 0.0f);

    tiny3d_VertexPos(dx , -dy, layer);
    tiny3d_VertexTexture(0.99f, 0.0f);

    tiny3d_VertexPos(dx , dy , layer);
    tiny3d_VertexTexture(0.99f, 0.99f);

    tiny3d_VertexPos(-dx, dy , layer);
    tiny3d_VertexTexture(0.0f , 0.99f);

    tiny3d_End();

    tiny3d_SetMatrixModelView(NULL); // set matrix identity

}

/*
 * Credits to Deroad (NoRSX)
 * https://github.com/wargio/NoRSX/blob/master/libNoRSX/Image.cpp
 */
void ScaleLine(u32 *Target, u32 *Source, u32 SrcWidth, u32 TgtWidth){
 //Thanks to: http://www.compuphase.com/graphic/scale.htm
	int NumPixels1 = TgtWidth;
	int IntPart1 = SrcWidth / TgtWidth;
	int FractPart1 = SrcWidth % TgtWidth;
	int E1 = 0;

	while (NumPixels1-- > 0) {
		*Target++ = *Source;
		Source += IntPart1;
		E1 += FractPart1;
		if (E1 >= TgtWidth) {
			E1 -= TgtWidth;
			Source++;
		} /* if */
	} /* while */
	return;
}

/*
 * Credits to Deroad (NoRSX)
 * https://github.com/wargio/NoRSX/blob/master/libNoRSX/Image.cpp
 */
jpgData ResizeJPG(jpgData jpg_in, u32 targetWidth, u32 targetHeight)
{
	jpgData jpg_out;
	if(jpg_in.bmp_out)
	{
		jpg_out.bmp_out = (u32 *) malloc(targetHeight * targetWidth * sizeof(u32));
		u32 *Source = (u32 *)(void *)jpg_in.bmp_out;
		u32 *Target = (u32 *)(void *)jpg_out.bmp_out;

		jpg_out.height = targetHeight;
		jpg_out.width  = targetWidth;
		jpg_out.pitch  = targetWidth * 4;

		int NumPixels0 = targetHeight;
		int IntPart0 = (jpg_in.height / targetHeight) * jpg_in.width;
		int FractPart0 = jpg_in.height % targetHeight;
		int E0 = 0;
		u32 *PrevSource = NULL;

		while (NumPixels0-- > 0) {
			if (Source == PrevSource) {
				memcpy(Target, Target-targetWidth, targetWidth * sizeof(*Target));
			} else {
				ScaleLine(Target, Source, jpg_in.width, targetWidth);
				PrevSource = Source;
			}
			Target += targetWidth;
			Source += IntPart0;
			E0 += FractPart0;
			if (E0 >= targetHeight) {
				E0 -= targetHeight;
				Source += jpg_in.width;
			}
		}
	}
	return jpg_out;
}


// Resets new frame, draws the texture_jpg, and, if showFPS is != 0, draws the FPS counter
void drawScene()
{
    
	
	/* DRAWING STARTS HERE */

    // clear the screen, buffer Z and initializes environment to 2D

    tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);

    // Enable alpha Test
    tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);

    // Enable alpha blending.
    tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
        NV30_3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | NV30_3D_BLEND_FUNC_DST_ALPHA_ZERO,
        TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
	
    tiny3d_Project2D(); // change to 2D context (remember you it works with 848 x 512 as virtual coordinates)
	
    // fix Perspective Projection Matrix
	
    //DrawBackground2D(0x0040FFFF); // light blue 
	
    count_frames++;
	
    //int cur_text = ghost[0].frame; // get current texture index for frame
	
	tiny3d_SetTexture(0, texture_jpg_offset, texture_jpg.width,
		texture_jpg.height, texture_jpg.pitch,  
		TINY3D_TEX_FORMAT_A8R8G8B8, TEXTURE_LINEAR);
	
	// draw sprite
	DrawSprites2D(0, 0, (float)0, texture_jpg.width, texture_jpg.height, 0xFFFFFFFF);
		
	//printf("pic res: %d by %d\n", texture_img.width, texture_img.height);
	
    if (showFPS)
	{
		char * frameStr = malloc(100);
		sprintf(frameStr, "FPS: %f", fps);
		
		SetFontSize(16, 32);
		SetFontColor(0xFFFFFFFF, 0x000000FF);
		SetFontAutoCenter(1);
		
		DrawString(30, 30, frameStr);
		
		free(frameStr);
		
		SetFontColor(0xFFFFFFFF, 0x00000000);
	}
	
	//if ((count_frames % 10) == 0)
	//	printf("frame: %d\n", count_frames);
    
}


// Used only in initialization. Allocates 64 mb for textures, loads the font, and loads the internet_jpg image
void LoadTexture()
{
    texture_mem = tiny3d_AllocTexture(64*1024*1024); // alloc 64MB of space for textures (this pointer can be global)
    
    u32 * texture_pointer; // use to asign texture space without changes texture_mem

    if(!texture_mem) return; // fail!

    texture_pointer = texture_mem;
	
	//printf("texture_pointer 1: 0x%08x\n", tiny3d_TextureOffset(texture_pointer));
	ResetFont();
	texture_pointer = (u32 *) AddFontFromBitmapArray((u8 *) font  , (u8 *) texture_pointer, 32, 255, 16, 32, 2, BIT0_FIRST_PIXEL);
	jpg_texture_mem = texture_pointer;
	//printf("texture_pointer 2: 0x%08x\n", tiny3d_TextureOffset(texture_pointer));
	
	jpgLoadFromBuffer((const void*)internet_jpg, internet_jpg_size, &texture_jpg);
	
	texture_jpg_offset = 0;
	
	// copy texture datas from PNG to the RSX memory allocated for textures
	if(texture_jpg.bmp_out)
	{
        memcpy(texture_pointer, texture_jpg.bmp_out, texture_jpg.pitch * texture_jpg.height);
        
        free(texture_jpg.bmp_out); // free the PNG because i don't need this datas
		
        texture_jpg_offset = tiny3d_TextureOffset(texture_pointer);      // get the offset (RSX use offset instead address)
		
        texture_pointer += ((texture_jpg.pitch * texture_jpg.height + 15) & ~15) / 4; // aligned to 16 bytes (it is u32) and update the pointer
    }
}

//Used to allocate the image sent from the PC
void LoadTextureIMG(char * img, int size)
{
    //u32 * texture_mem = tiny3d_AllocTexture(64*1024*1024); // alloc 64MB of space for textures (this pointer can be global)
    
    u32 * texture_pointer; // use to asign texture space without changes texture_mem

    if(!texture_mem || !jpg_texture_mem) return; // fail!

    texture_pointer = jpg_texture_mem;
	
	jpgLoadFromBuffer(img, size, &texture_jpg);
	
	// copy texture datas from PNG to the RSX memory allocated for textures
    texture_jpg_offset = 0;
    
    if(texture_jpg.bmp_out)
	{
        memcpy(texture_pointer, texture_jpg.bmp_out, texture_jpg.pitch * texture_jpg.height);
        
        free(texture_jpg.bmp_out); // free the PNG because i don't need this datas
		
        texture_jpg_offset = tiny3d_TextureOffset(texture_pointer);      // get the offset (RSX use offset instead address)

        texture_pointer += ((texture_jpg.pitch * texture_jpg.height + 15) & ~15) / 4; // aligned to 16 bytes (it is u32) and update the pointer
    }
}

//Takes the raw compressed bytes, decompresses it, resizes it, and loads it into texture_jpg
void LoadTextureResizeJPG(char * img, int size, int width, int height)
{
    //u32 * texture_mem = tiny3d_AllocTexture(64*1024*1024); // alloc 64MB of space for textures (this pointer can be global)
    
    u32 * texture_pointer; // use to asign texture space without changes texture_mem

    if(!texture_mem || !jpg_texture_mem) return; // fail!

    texture_pointer = jpg_texture_mem;
	
	jpgLoadFromBuffer(img, size, &texture_jpg);
	
	
	// copy texture datas from PNG to the RSX memory allocated for textures
    texture_jpg_offset = 0;
    
    if(texture_jpg.bmp_out)
	{
		jpgData newJPG = ResizeJPG(texture_jpg, width, height);
		free(texture_jpg.bmp_out);
		texture_jpg = newJPG;
		
        memcpy(texture_pointer, texture_jpg.bmp_out, texture_jpg.pitch * texture_jpg.height);
        
        free(texture_jpg.bmp_out); // free the PNG because i don't need this datas
		
        texture_jpg_offset = tiny3d_TextureOffset(texture_pointer);      // get the offset (RSX use offset instead address)

        texture_pointer += ((texture_jpg.pitch * texture_jpg.height + 15) & ~15) / 4; // aligned to 16 bytes (it is u32) and update the pointer
    }
}


void exiting()
{

    sysModuleUnload(SYSMODULE_JPGDEC);
  
}

// Initializes a server and waits for the PC to connect. Returns the new socket when connected.
int startServer()
{
	printf("Creating TCP socket...\n");

	socklen_t clilen;
	originalSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	//originalSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (originalSocket < 0) {
		printf("Unable to create a socket: %d\n", errno);
		return originalSocket;
	}
	printf("Socket created: %d\n", originalSocket);

	struct sockaddr_in server, cli_addr;
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(portno);
	
	if (bind(originalSocket, (struct sockaddr*) &server, sizeof(server)) < 0)
	{
		printf("Failed to bind socket");
		return -1;
	}
	
	listen(originalSocket, 5);
	clilen = sizeof(cli_addr);
	int newsockfd = accept(originalSocket, (struct sockaddr *) &cli_addr, &clilen);
	
	return newsockfd;
}

// Takes the analog stick's value and returns whether it is Left/Up, Right/Down, or neither
char parseAnalog(int val)
{
	if (val > 0xC0)
		return ANA_Right_Down;
	else if (val < 0x40)
		return ANA_Left_Up;
	
	return 0;
}

//Converts the padData to a byte array to be sent to the PS3
char * padDataToPacket(padData pad)
{
	char * ret = malloc(20);
	
	ret[0] = pad.BTN_CROSS;
	ret[1] = pad.BTN_TRIANGLE;
	ret[2] = pad.BTN_CIRCLE;
	ret[3] = pad.BTN_SQUARE;
	
	ret[4] = pad.BTN_LEFT;
	ret[5] = pad.BTN_UP;
	ret[6] = pad.BTN_RIGHT;
	ret[7] = pad.BTN_DOWN;
	
	ret[8] = pad.BTN_R1;
	ret[9] = pad.BTN_R2;
	ret[10] = pad.BTN_L1;
	ret[11] = pad.BTN_L2;
	ret[12] = pad.BTN_R3;
	ret[13] = pad.BTN_L3;
	
	ret[14] = pad.BTN_START;
	ret[15] = pad.BTN_SELECT;
	
	//Left Horizontal
	ret[16] = parseAnalog(pad.ANA_L_H);
	//Left Vertical
	ret[17] = parseAnalog(pad.ANA_L_V);
	//Right Horizontal
	ret[18] = parseAnalog(pad.ANA_R_H);
	//Right Vertical
	ret[19] = parseAnalog(pad.ANA_R_V);
	
	//printf("R_H: %d, R_V: %d, L_H: %d, L_V: %d\n", pad.ANA_R_H, pad.ANA_R_V, pad.ANA_L_H, pad.ANA_L_V);
	
	return ret;
}

/*
 * recv with more modes
 */
int _recv(int s, char *buf, int len, int flags, int mode) {
	int x = 0, y = 1;

	switch (mode) {
		case 0: // recv in one go
			return recv(s, buf, len, flags);
		case 1: // recv byte by byte
			while (y != 0) {
				y = recv(s, buf + x, 1, flags);
				x += y;
			}
			return x;
		case 2: // recv byte by byte until len is reached
			while (x < len) {
				x += recv(s, buf + x, 1, flags);
			}
			return x;
		case 3: /* recv 1024 bytes chunks until len is reached */
			y = 0;
			x = 1024;
			while (y < len) {
				if (y >= (len - (len % 1024))) /* If true, then receive the final chunk of size (len % 1024) */
					x = len % 1024;

				y += recv(s, buf + y, x, flags);
			}
			return y;
		case 4: /* recv flags bytes chunks until len is reached */
			y = 0;
			x = flags;
			while (y < len) {
				if (y >= (len - (len % flags))) /* If true, then receive the final chunk of size (len % flags) */
					x = len % flags;

				y += recv(s, buf + y, x, 0);
			}
			return y;
	}
	return 0;
}

/*
 * send with more modes
 */
int _send(int s, char *buf, int len, int flags, int mode) {
	int x = 0, off = 0;

	switch (mode) {
		case 0: // send in one go
			return send(s, buf, len, flags);
			break;
		case 1: // send byte by byte
			while (x <= len) {
				x += send(s, buf + x, 1, flags);
			}
			break;
		case 2: // send in chunks specified by len
			while ((x = send(s, buf + off, len, flags)) > 0) {
				off += x;
				len -= x;
			}
	}
	return 0;
}

// Sends the string "K" to the PC
void replyWithOkay(int socket)
{
	char * buf = malloc(2);
	buf[0] = 'K';
	buf[1] = '\0';
	_send(socket, buf, 2, 0, 0);
	free(buf);
}

/*
	Main function
	
	Initializes pad, screen, textures, and server
	
	When connected, it loops and parses PC communication
*/
s32 main(s32 argc, const char* argv[])
{
	time_t starttime = 0;
	float oldDifTime = 0;
	padInfo padinfo;
	padData paddata[MAX_PADS];
	int i;

	tiny3d_Init(1024*1024);

	ioPadInit(7);
    
    //sysModuleLoad(SYSMODULE_PNGDEC);
	sysModuleLoad(SYSMODULE_JPGDEC);

    atexit(exiting); // Tiny3D register the event 3 and do exit() call when you exit  to the menu

	// Load texture

    LoadTexture();
	
	SetFontSize(16, 32);
    SetFontColor(0xFFFFFFFF, 0x00000000);
    SetFontAutoCenter(1);
	
	//texture_mem = tiny3d_AllocTexture(64*1024*1024);

	videoState state;
	assert(videoGetState(0, 0, &state) == 0); // Get the state of the display
	assert(state.state == 0); // Make sure display is enabled
	
	videoResolution res;
	assert(videoGetResolution(state.displayMode.resolution, &res) == 0);
	printf("Resolution: %d by %d\n", res.width, res.height);
	screen_width = res.width;
	screen_height = res.height;
	
	initLabel:
	
	tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
    tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);
    tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
        NV30_3D_BLEND_FUNC_DST_RGB_ONE_MINUS_SRC_ALPHA | NV30_3D_BLEND_FUNC_DST_ALPHA_ZERO,
        TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
	tiny3d_Project2D();
    DrawBackground2D(0x0040FFFF); //light blue
	
	DrawString(10, 100, "Application Controller PS3");
	DrawString(10, 150, "by Dnawrkshp");
	
	DrawString(10, 300, "Waiting for PC connection...");
	
	tiny3d_Flip();
	
	int socket = startServer();
	if (socket <= 0)
		return 0;
	
	// Ok, everything is setup. Now for the main loop.
	int conStatus = 0;
	
	starttime = time (NULL);
	while (conStatus >= 0) {
		
		/* Recieve input from PC */
		char * header = malloc(8);
		conStatus = _recv(socket, header, 8, 0, 0);
		
		/* Parse header */
		if (header[0] == requestPad)
		{
			/*
			 * Header format:
			 * 0 - ID (requestPad)
			 * 1 - Controller number (index 0)
			 */
			
			int port = (int)header[1];
			
			// Check the pads.
			ioPadGetInfo(&padinfo);

			//for(i = 0; i < MAX_PADS; i++){

				if(padinfo.status[port]){
					ioPadGetData(port, &paddata[port]);

					//if(paddata[i].BTN_R3 && paddata[i].BTN_L3)
					//{
					//	return 0;
					//}				
				}

			//}
			
			char * buf = padDataToPacket(paddata[port]);
			_send(socket, buf, 20, 0, 0);
			free(buf);
		}
		else if (header[0] == updateImage)
		{
			/*
			 * Header format:
			 * 0 - ID (updateImage)
			 * 4..7 - Size of image
			 */
			
			//isJPG = header[1];
			
			int size = ((int)header[4] << 24) | ((int)header[5] << 16) | ((int)header[6] << 8) | (int)header[7];
			//printf("size: %d bytes, %f kb\n", size, (float)size / 1024);
			
			//memset(recvImage, 0, MAX_RECV_IMAGE_SIZE);
			
			char * image = malloc(size);
			conStatus = _recv(socket, image, size, 4096, 4);
			
			/* Copy image to printed image buffer */
			LoadTextureIMG(image, size);
			
			//printf("recieved image!\n");
			
			replyWithOkay(socket);
			
			free(image);
			
			drawScene(); // Draw

			/* DRAWING FINISH HERE */

			tiny3d_Flip();
		}
		else if (header[0] == updateImageZLIB)
		{
			/*
			 * Header format:
			 * 0 - ID (updateImage)
			 * 1..3 - Size of compressed image
			 * 4..7 - Size of uncompressed image
			 */
			
			
			int size = ((int)header[4] << 24) | ((int)header[5] << 16) | ((int)header[6] << 8) | (int)header[7];
			int compSize = ((int)header[1] << 16) | ((int)header[2] << 8) | (int)header[3];
			//printf("size: %d bytes, %f kb\n", size, (float)size / 1024);
			
			//memset(recvImage, 0, MAX_RECV_IMAGE_SIZE);
			
			char * compImage = malloc(compSize);
			conStatus = _recv(socket, compImage, compSize, 10240, 4);
			
			char * image = malloc(size);
			uLongf decompSize = (uLongf)size;
			uncompress((Bytef *)image, &decompSize, (Bytef *)compImage, compSize);
			
			/* Copy image to printed image buffer */
			
			//LoadTextureIMG(image, size);
			LoadTextureResizeJPG(image, size, 848, 512);
			
			//printf("recieved image!\n");
			
			replyWithOkay(socket);
			
			//printf("compSize %d, size %d\n", compSize, size);
			
			free(image);
			free(compImage);
			
			drawScene(); // Draw

			/* DRAWING FINISH HERE */

			tiny3d_Flip();
		}
		else if (header[0] == updateShowFPS)
		{
			/*
			 * Header format:
			 * 0 - ID (updateImage)
			 * 1 - showFPS value
			 */
			
			showFPS = header[1];
		}
		else if (header[0] == exitToXMB)
		{
			goto exitLabel;
		}
		else if (header[0] == resetToListen)
		{
			free(header);
			close(socket);
			close(originalSocket);
			count_frames = 0;
			goto initLabel;
		}
		
		float difTime = (float)difftime(time (NULL), starttime);
		if (oldDifTime != difTime)
		{
			oldDifTime = difTime;
			if (difTime > 2 && count_frames > 0)
				fps = (float)count_frames / difTime;
			if (((difTime / 2) == (float)((int)difTime / 2)) && !showFPS)
				printf("FPS: %f\n", fps);
			if (difTime >= 15)
			{
				//printf("(float)difftime(time (NULL), starttime) = %f\n", (float)difftime(time (NULL), starttime));
				count_frames = 0;
				starttime = time(NULL);
			}
		}
		
		free(header);
	
	}
	
	exitLabel:
	
	close(socket);
	close(originalSocket);
	
	return 0;
}
