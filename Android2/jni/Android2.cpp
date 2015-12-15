/*
* Copyright (C) 2009 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

// OpenGL ES 2.0 code

#include <jni.h>
#include <android/log.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <string>
#include <glm\glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include "obj.inl"
#include "objball.inl"

namespace {
	GLuint gProgram;
	GLuint gProgram2;
	GLuint gvPositionHandle;
	GLuint gvPositionHandle2;
	GLuint gvUvHandle;
	GLuint gvTextureHandle;
	GLuint gvMVPHandle;
	GLuint gvMVPHandle2;
	GLuint gvMVHandle;
	GLuint gvnormalMatrixHandle;
	GLuint gvLHandle;
	GLuint gvNormalHandle;
	//GLuint gvTangentHandle;
	//GLuint gvBitangentHandle;	

	GLuint Diffuse_mapID;
	GLuint diffusemap;

	glm::mat4 V;
	glm::mat4 P;
	glm::mat4 VP;
	glm::mat4 MVP(1.0);
	glm::mat4 MV(1.0);
	glm::mat4 M(1.0);
	glm::mat3 normalMatrix(1.0);
	glm::vec3 L(1.0);

	GLfloat alpha = 0.0f;
	GLfloat wcpp = 0.0f;
	GLfloat hcpp = 0.0f;

	GLuint vertexbuffer;
	GLuint vertexbuffer2;
	GLuint uvbuffer;
	GLuint normalbuffer;
	GLuint tangentbuffer;
	GLuint bitangentbuffer;
	GLuint indexbuffer;

	GLuint VertexArrayID;

	int sizeOfVArray;
	int sizeOfVArray2;
	int sizeOfUArray;

	bool breeth = true;
}

#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

static void printGLString(const char *name, GLenum s) {
	const char *v = (const char *)glGetString(s);
	LOGI("GL %s = %s\n", name, v);
}

static void checkGlError(const char* op) {
	for (GLint error = glGetError(); error; error
		= glGetError()) {
		LOGI("after %s() glError (0x%x)\n", op, error);
	}
}

static const char gVertexShader[] =
"attribute vec3 myVertex;\n"
"attribute vec2 vertexUV;\n"
"attribute vec3 myNormal;\n"
"varying vec2 UV;\n"
"varying vec3 Normal;\n"
"varying vec3 Position;\n"
"varying vec3 LightPos;\n"
"uniform mat4 MVP;\n"
"uniform mat4 MV;\n"
"uniform vec3 L;\n"
"uniform mat3 normalMatrix;\n"
"void main() {\n"
"  vec4 vPos = vec4(myVertex,1);\n"
"  Position = vec3(MV * vPos);\n"
"  UV = vertexUV;\n"
"  Normal = normalize(normalMatrix*myNormal);\n"
"  gl_Position = MVP * vPos;\n"
"  LightPos = L;\n"
"}\n";

//"attribute vec2 vertexUV;\n"
//"varying vec2 UV;\n"
//"  UV = vertexUV;\n"


static const char gFragmentShader[] = // TODO: Fix the texture usage.
"precision mediump float;\n"
"varying vec2 UV;\n"
"varying vec3 Normal;\n"
"varying vec3 Position;\n"
"varying vec3 LightPos;\n"
"uniform sampler2D mytexture;\n"
"void main() {\n"
"  vec3 normal = normalize(Normal);\n"
"  vec3 color = vec3(texture2D(mytexture, UV));\n"
"  vec3 ambient = 0.2*color;\n"
"  vec3 lightVector = normalize(LightPos - Position);\n"
"  float distance = length(LightPos - Position);\n"
"  float attenuation = 1.0 / (1.0 + 0.0000009 * distance + 0.005 * (distance * distance));\n"
"  float diff = max(dot(normal, lightVector), 0.0);\n"
"  vec3 diffuse = diff * color;\n"
"  gl_FragColor = vec4(diffuse*attenuation + ambient*attenuation, 1.0);\n"
"}\n";

//"  vec3 normal = normalize(Normal);\n"
//"  vec3 color = vec3(0.5,1.0,0.5);\n"
//"  vec3 ambient = 0.2*color;\n"
//"  vec3 lightVector = normalize(LightPos - Position);\n"
//"  float distance = length(LightPos - Position);\n"
//"  float attenuation = 1.0 / (1.0 + 0.0000009 * distance + 0.0016 * (distance * distance));\n"
//"  float diff = max(dot(normal, lightVector), 0.0);\n"
//"  vec3 diffuse = diff * color;\n"
//"  gl_FragColor = vec4(diffuse*attenuation + ambient*attenuation, 1.0);\n"

//"  gl_FragColor = vec4(1.0,0.5,0.2,1.0);\n"
//"  gl_FragColor = vec4(Normal,1.0);\n"
//"  gl_FragColor = texture2D(mytexture, UV);\n"

static const char gShadelessVertexShader[] =
"attribute vec3 myVertex;\n"
"uniform mat4 MVP;\n"
"void main() {\n"
"  vec4 vPos = vec4(myVertex,1);\n"
"  gl_Position = MVP * vPos;\n"
"}\n";

static const char gShadelessFragmentShader[] =
"precision mediump float;\n"
"void main() {\n"
"  gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
"}\n";

GLuint loadShader(GLenum shaderType, const char* pSource) {
	GLuint shader = glCreateShader(shaderType);
	if (shader) {
		glShaderSource(shader, 1, &pSource, NULL);
		glCompileShader(shader);
		GLint compiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (!compiled) {
			GLint infoLen = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
			if (infoLen) {
				char* buf = (char*)malloc(infoLen);
				if (buf) {
					glGetShaderInfoLog(shader, infoLen, NULL, buf);
					LOGE("Could not compile shader %d:\n%s\n",
						shaderType, buf);
					free(buf);
				}
				glDeleteShader(shader);
				shader = 0;
			}
		}
	}
	return shader;
}

GLuint createProgram(const char* pVertexSource, const char* pFragmentSource) {
	GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
	if (!vertexShader) {
		return 0;
	}

	GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
	if (!pixelShader) {
		return 0;
	}

	GLuint program = glCreateProgram();
	if (program) {
		glAttachShader(program, vertexShader);
		checkGlError("glAttachShader");
		glAttachShader(program, pixelShader);
		checkGlError("glAttachShader");

		glLinkProgram(program);
		GLint linkStatus = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
		if (linkStatus != GL_TRUE) {
			GLint bufLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
			if (bufLength) {
				char* buf = (char*)malloc(bufLength);
				if (buf) {
					glGetProgramInfoLog(program, bufLength, NULL, buf);
					LOGE("Could not link program:\n%s\n", buf);
					free(buf);
				}
			}
			glDeleteProgram(program);
			program = 0;
		}
	}
	return program;
}

GLuint generateTexture()
{	
	glGenTextures(1, &diffusemap);
	unsigned char pixels[] = {
		0, 20, 25, 255, 0, 0, 205, 142, 12,
		0, 0, 255, 255, 255, 255, 0, 255, 0,
		20, 205, 25, 0, 255, 0, 25, 0, 0
	};

	glActiveTexture(GL_TEXTURE0);
	checkGlError("glActiveTexture");
	glBindTexture(GL_TEXTURE_2D, diffusemap);
	checkGlError("glBindTexture");

	glTexImage2D(GL_TEXTURE_2D,
		0, 
		GL_RGB, 
		3, 
		3, 
		0, 
		GL_RGB, 
		GL_UNSIGNED_BYTE,
		pixels);
	checkGlError("glTexImage2D");
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	return 0;
}

void InitObject()
{
	sizeOfVArray = (sizeof(Vertices) / sizeof(*Vertices)) / 3;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeOfVArray* sizeof(glm::vec3), &Vertices[0], GL_STATIC_DRAW);

	sizeOfUArray = (sizeof(Uvs) / sizeof(*Uvs)) / 2;
	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeOfUArray*sizeof(glm::vec2), &Uvs[0], GL_STATIC_DRAW);

	int sizeOfNArray = (sizeof(Normals) / sizeof(*Normals)) / 3;
	glGenBuffers(1, &normalbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeOfNArray*sizeof(glm::vec3), &Normals[0], GL_STATIC_DRAW);

	/*glBindBuffer(GL_ARRAY_BUFFER, tangentbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Tangents), &Tangents[0], GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, bitangentbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Bitangents), &Bitangents[0], GL_STATIC_DRAW);*/

	gvPositionHandle = glGetAttribLocation(gProgram, "myVertex");
	checkGlError("glGetAttribLocation");
	LOGI("glGetAttribLocation(\"myVector\") = %d\n",
		gvPositionHandle);

	gvUvHandle = glGetAttribLocation(gProgram, "vertexUV");
	checkGlError("glGetAttribLocation");
	LOGI("glGetAttribLocation(\"vertexUV\") = %d\n",
		gvUvHandle);

	gvNormalHandle = glGetAttribLocation(gProgram, "myNormal");
	checkGlError("glGetAttribLocation");
	LOGI("glGetAttribLocation(\"myVector\") = %d\n",
		gvNormalHandle);

	gvMVPHandle = glGetUniformLocation(gProgram, "MVP");
	checkGlError("glGetUniformLocation");
	LOGI("glGetUniformLocation(\"MVP\") = %d\n",
		gvMVPHandle);

	gvMVHandle = glGetUniformLocation(gProgram, "MV");
	checkGlError("glGetUniformLocation");
	LOGI("glGetUniformLocation(\"MV\") = %d\n",
		gvMVHandle);

	gvnormalMatrixHandle = glGetUniformLocation(gProgram, "normalMatrix");
	checkGlError("glGetUniformLocation");
	LOGI("glGetUniformLocation(\"normalMatrix\") = %d\n",
		gvnormalMatrixHandle);


	gvLHandle = glGetUniformLocation(gProgram, "L");
	checkGlError("glGetUniformLocation");
	LOGI("glGetUniformLocation(\"L\") = %d\n",
		gvLHandle);

	gvTextureHandle = glGetUniformLocation(gProgram, "mytexture");
	checkGlError("glGetUniformLocation");
	LOGI("glGetUniformLocation(\"mytexture\") = %d\n",
		gvTextureHandle);

	generateTexture();
}

void DrawObject(glm::vec3 position, float rotation, glm::vec3 rotationaxel)
{
	M = glm::translate(position)*glm::rotate(rotation, rotationaxel);
	MVP = VP * M;

	MV = V * M;

	normalMatrix = glm::inverse(glm::transpose(glm::mat3(M)));

	glUseProgram(gProgram);
	checkGlError("glUseProgram");

	glUniformMatrix4fv(gvMVPHandle, 1, GL_FALSE, &MVP[0][0]);
	checkGlError("glUniformMatrix4fv");

	glUniformMatrix4fv(gvMVHandle, 1, GL_FALSE, &MV[0][0]);
	checkGlError("glUniformMatrix4fv");

	glUniformMatrix3fv(gvnormalMatrixHandle, 1, GL_FALSE, &normalMatrix[0][0]);
	checkGlError("glUniformMatrix3fv");

	glUniform3fv(gvLHandle, 1, &L[0]);
	checkGlError("glUniform3fv");

	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glVertexAttribPointer(
		gvPositionHandle, //layout in the shader.
		3,       // size
		GL_FLOAT,// type
		GL_FALSE,// normalized
		0,       // stride
		(void*)0 // array buffer offset
		);

	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glVertexAttribPointer(
		gvUvHandle, //layout in the shader.
		2,       // size
		GL_FLOAT,// type
		GL_FALSE,// normalized
		0,       // stride
		(void*)0 // array buffer offset
		);

	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glVertexAttribPointer(
		gvNormalHandle, //layout in the shader.
		3,       // size
		GL_FLOAT,// type
		GL_FALSE,// normalized
		0,       // stride
		(void*)0 // array buffer offset
		);

	glEnableVertexAttribArray(gvPositionHandle);
	glEnableVertexAttribArray(gvUvHandle);
	glEnableVertexAttribArray(gvNormalHandle);
	checkGlError("glEnableVertexAttribArray");

	glActiveTexture(GL_TEXTURE0);
	checkGlError("glActiveTexture");
	glBindTexture(GL_TEXTURE_2D, diffusemap);
	checkGlError("glBindTexture");
	glUniform1i(gvTextureHandle, 0);
	checkGlError("glUniform1i");

	glDrawArrays(GL_TRIANGLES, 0, sizeOfVArray);
	checkGlError("glDrawArrays");
	glDisableVertexAttribArray(gvPositionHandle);
	glDisableVertexAttribArray(gvUvHandle);
	glDisableVertexAttribArray(gvNormalHandle);

	//glDrawElements(GL_LINES, sizeOfVArray, GL_UNSIGNED_SHORT, Indices);
}

void InitLightObject()
{
	sizeOfVArray2 = (sizeof(BallVertices) / sizeof(*BallVertices)) / 3;
	glGenBuffers(1, &vertexbuffer2);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer2);
	glBufferData(GL_ARRAY_BUFFER, sizeOfVArray2* sizeof(glm::vec3), &BallVertices[0], GL_STATIC_DRAW);

	gvPositionHandle2 = glGetAttribLocation(gProgram2, "myVertex");
	checkGlError("glGetAttribLocation");
	LOGI("glGetAttribLocation(\"myVector\") = %d\n",
		gvPositionHandle2);

	gvMVPHandle2 = glGetUniformLocation(gProgram2, "MVP");
	checkGlError("glGetUniformLocation");
	LOGI("glGetUniformLocation(\"MVP\") = %d\n",
		gvMVPHandle2);
}

void DrawLightObject(glm::vec3 position, float rotation, glm::vec3 rotationaxel)
{
	M = glm::translate(position)*glm::rotate(rotation, rotationaxel);
	MVP = VP * M;

	glUseProgram(gProgram2);
	checkGlError("glUseProgram");

	glUniformMatrix4fv(gvMVPHandle2, 1, GL_FALSE, &MVP[0][0]);
	checkGlError("glUniformMatrix4fv");

	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer2);
	glVertexAttribPointer(
		gvPositionHandle2, //layout in the shader.
		3,       // size
		GL_FLOAT,// type
		GL_FALSE,// normalized
		0,       // stride
		(void*)0 // array buffer offset
		);	

	glEnableVertexAttribArray(gvPositionHandle2);
	checkGlError("glEnableVertexAttribArray");

	glDrawArrays(GL_TRIANGLES, 0, sizeOfVArray2);
	checkGlError("glDrawArrays");
	glDisableVertexAttribArray(gvPositionHandle2);

	//glDrawElements(GL_LINES, sizeOfVArray, GL_UNSIGNED_SHORT, Indices);
}


bool setupGraphics(int w, int h) {
	wcpp = w;
	hcpp = h;
	printGLString("Version", GL_VERSION);
	printGLString("Vendor", GL_VENDOR);
	printGLString("Renderer", GL_RENDERER);
	printGLString("Extensions", GL_EXTENSIONS);

	LOGI("setupGraphics(%d, %d)", w, h);
	gProgram = createProgram(gVertexShader, gFragmentShader);
	if (!gProgram) {
		LOGE("Could not create program.");
		return false;
	}

	gProgram2 = createProgram(gShadelessVertexShader, gShadelessFragmentShader);
	if (!gProgram2) {
		LOGE("Could not create program.");
		return false;
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glEnable(GL_TEXTURE_2D);

	InitObject();
	InitLightObject();

	
	glViewport(0, 0, w, h);
	checkGlError("glViewport");

	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	glDepthRangef(0.0, 100.0);
	glEnable(GL_DEPTH_TEST);

	return true;
}

void renderFrame() {
	// Backround
	static float grey = 0.0f;
	/*if (grey <= 0.3f && breeth == true) {
		grey += 0.001f;
		if (grey >= 0.3)
		{
			breeth = false;
		}
	}
	else if (breeth == false)
	{
		grey -= 0.001f;
		if (grey <= 0)
		{
			breeth = true;
		}
	}*/
	glClearColor(grey, grey, grey, 1.0f);
	checkGlError("glClearColor");

	// Clear
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	checkGlError("glClear");

	// Camera
	glm::vec3 cameraPos = glm::vec3(1.5f, 0.0f, 4.5f);
	glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

	P = glm::perspective(45.0f, wcpp / hcpp, 0.1f, 1000.f);
	V = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

	VP = P*V;

	// Light position
	L = glm::vec3(4.0f, 4.0f, (-7.0f + 14.0f * glm::cos(alpha))); //Light position
	DrawLightObject(L, alpha, glm::vec3(1.0f, 1.0f, 1.0f));

	// Objects
	for (int i = 0; i < 40; i++)
	{
		DrawObject(glm::vec3(((i*i) / 40.0f) * glm::sin(alpha) * 1.2f + i*0.7f, (((i*i) / 20.0f) * glm::cos(alpha) * 0.6f), (-i  * 3.0f)), (i + 1) * alpha, glm::vec3(0.0f, 1.0f, 1.0f));
	}

	alpha += 0.005f;
}

extern "C" {
	JNIEXPORT void JNICALL Java_com_gles_pt_GL2JNILib_init(JNIEnv * env, jobject obj, jint width, jint height);
	JNIEXPORT void JNICALL Java_com_gles_pt_GL2JNILib_step(JNIEnv * env, jobject obj);
};

JNIEXPORT void JNICALL Java_com_gles_pt_GL2JNILib_init(JNIEnv * env, jobject obj, jint width, jint height)
{
	setupGraphics(width, height);
}

JNIEXPORT void JNICALL Java_com_gles_pt_GL2JNILib_step(JNIEnv * env, jobject obj)
{
	renderFrame();
}
