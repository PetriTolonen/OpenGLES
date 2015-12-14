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

namespace {
	GLuint gProgram;
	GLuint gvPositionHandle;
	GLuint gvUvHandle;
	GLuint gvTextureHandle;
	GLuint gvMVPHandle;
	GLuint gvMVHandle;
	GLuint gvLHandle;
	GLuint gvNormalHandle;
	//GLuint gvTangentHandle;
	//GLuint gvBitangentHandle;	

	GLuint Diffuse_mapID;

	glm::mat4 V;
	glm::mat4 P;
	glm::mat4 VP;
	glm::mat4 MVP(1.0);
	glm::mat4 MV(1.0);
	glm::mat4 M(1.0);
	glm::vec3 L(1.0);

	GLfloat alpha = 0.0f;
	GLfloat wcpp = 0.0f;
	GLfloat hcpp = 0.0f;

	GLuint vertexbuffer;
	GLuint uvbuffer;
	GLuint normalbuffer;
	GLuint tangentbuffer;
	GLuint bitangentbuffer;
	GLuint indexbuffer;

	GLuint VertexArrayID;

	int sizeOfVArray;
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
"void main() {\n"
"  vec4 vPos = vec4(myVertex,1);\n"
"  Position = vec3(MV * vPos);\n"
"  UV = vertexUV;\n"
"  Normal = vec3(MVP*vec4(myNormal, 0.0));\n"
"  gl_Position = MVP * vPos;\n"
"  LightPos = L;"
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
"  vec3 lightVector = normalize(LightPos - Position);\n"
"  float diffuse = max(dot(Normal, lightVector), 0.0);\n"
"  diffuse = diffuse + 0.3;\n"
"  gl_FragColor = (diffuse*vec4(1.0,0.5,0.2,1.0));\n"
"}\n";

//"  gl_FragColor = vec4(1.0,0.5,0.2,1.0);\n"
//"  gl_FragColor = vec4(Normal,1.0);\n"
//"  gl_FragColor = texture2D(mytexture, UV);\n"

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

GLuint generateTexture(GLuint _ID)
{	
	float pixels[] = {
		1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
	};

	glActiveTexture(GL_TEXTURE0);
	checkGlError("glActiveTexture");
	glBindTexture(GL_TEXTURE_2D, _ID);
	checkGlError("glBindTexture");

	glTexImage2D(GL_TEXTURE_2D,
		0, 
		GL_RGB, 
		2, 
		2, 
		0, 
		GL_RGB, 
		GL_FLOAT,
		pixels);
	checkGlError("glTexImage2D");
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	return _ID;
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

	gvLHandle = glGetUniformLocation(gProgram, "L");
	checkGlError("glGetUniformLocation");
	LOGI("glGetUniformLocation(\"L\") = %d\n",
		gvLHandle);

	gvTextureHandle = glGetAttribLocation(gProgram, "mytexture");
	checkGlError("glGetAttribLocation");
	LOGI("glGetAttribLocation(\"mytexture\") = %d\n",
		gvTextureHandle);

	glGenTextures(1, &Diffuse_mapID);
	Diffuse_mapID = generateTexture(Diffuse_mapID);
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

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glEnable(GL_TEXTURE_2D);

	InitObject();

	glViewport(0, 0, w, h);
	checkGlError("glViewport");

	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);

	return true;
}

void DrawObject(glm::vec3 position, float rotation, glm::vec3 rotationaxel)
{
	M = glm::translate(position)*glm::rotate(rotation, rotationaxel);
	MVP = VP * M;

	MV = V * M;

	glUseProgram(gProgram);
	checkGlError("glUseProgram");

	glUniformMatrix4fv(gvMVPHandle, 1, GL_FALSE, &MVP[0][0]);
	checkGlError("glUniformMatrix4fv");

	glUniformMatrix4fv(gvMVHandle, 1, GL_FALSE, &MV[0][0]);
	checkGlError("glUniformMatrix4fv");

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
	glBindTexture(GL_TEXTURE_2D, Diffuse_mapID);
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

void renderFrame() {
	static float grey = 0.0f;
	if (grey <= 0.3f && breeth == true) {
		grey += 0.0008f;
		if (grey >= 0.3)
		{
			breeth = false;
		}
	}
	else if (breeth == false)
	{
		grey -= 0.0008f;
		if (grey <= 0)
		{
			breeth = true;
		}
	}
	glClearColor(grey, grey, grey, 1.0f);
	checkGlError("glClearColor");
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	checkGlError("glClear");

	glm::vec3 cameraPos = glm::vec3(1.5f, 0.0f, 4.5f);
	glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

	P = glm::perspective(45.0f, 4.0f / 3.0f, 0.1f, 10000.f);
	V = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

	VP = P*V;

	L = glm::vec3(4.0f, 4.0f, (-7.0f + 14.0f * glm::cos(alpha))); //Light position

	for (int i = 0; i < 1000; i++)
	{
		DrawObject(glm::vec3(((i*i) / 40.0f) * glm::sin(alpha) * 1.2f + i*0.7f, (((i*i) / 20.0f) * glm::cos(alpha) * 0.6f), (-i  * 3.0f)), (i + 1) * alpha, glm::vec3(0.0f, 1.0f, 1.0f));
	}

	alpha += 0.01f;
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
