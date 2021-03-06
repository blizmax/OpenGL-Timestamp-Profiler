// main.cpp

#include <stdio.h>
#include "scene.h"

#include <GL/glew.h>

#ifdef __APPLE__
	#include <OpenGL/gl.h>
	#include <OpenGL/glu.h>
#else
	#include <GL/gl.h>
	#include <GL/glu.h>
#endif

#include <GL/glfw.h>
#include <stdlib.h>
#include <math.h>
#include "hp_timer.h"
#include "profiler.h"
#include "drawer2D.h"
#include "thread.h"
#include "math_utils.h"

//#define USE_FORWARD_COMPATIBLE_CONTEXT_GL_3_3
//#define USE_FORWARD_COMPATIBLE_CONTEXT_GL_4
//#define USE_DEBUG_CONTEXT

#define BASE_TITLE	"Profiler - OpenGL Insights"
#define WIN_WIDTH	640
#define WIN_HEIGHT	480

static volatile bool	done = false;
Scene					scene;
bool					help_visible = true;

void GLFWCALL onMouseClick(int x, int y);
void GLFWCALL onKey(int key, int action);
void drawHelp();

void debugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
				   GLsizei length, const GLchar* message, GLvoid* userParam)
{
	fprintf(stderr, "GL debug: %s\n", message);
}

void fpsCount(const char* base_title)
{
	static bool first_time = false;
	static double t, t0;
	static int fps = 0, frames = 0;

	if(first_time)
	{
		t0 = glfwGetTime();
		first_time = true;
	}

	t = glfwGetTime();

	if( (t-t0) > 1.0)
	{
		fps = (int)((double)frames / (double)(t-t0));
		char buffer[256] = "";
		sprintf(buffer, "%s - FPS : %d", base_title, fps);
		glfwSetWindowTitle(buffer);
		t0 = t;
		frames = 0;
	}
	frames++;
}

int main()
{
	int win_w, win_h;
	int prev_win_w, prev_win_h;
	int mouse_x, mouse_y;
	int prev_mouse_x, prev_mouse_y;

	initTimer();

	// Initialize GLFW
	if( !glfwInit() )
	{
		fprintf( stderr, "Failed to initialize GLFW\n" );
		return EXIT_FAILURE;
	}

	// Open a window and create its OpenGL context
#ifdef USE_FORWARD_COMPATIBLE_CONTEXT_GL_3_3
	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR,  3);
	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR,  3);
	glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #ifdef __APPLE__
        #warning "MacOS X might not yet support OpenGL 3.3 (3.2 was the maximum for 10.8)"
    #endif
#elif defined USE_FORWARD_COMPATIBLE_CONTEXT_GL_4
	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR,  4);
	glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR,  0);
	glfwOpenWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	#ifdef __APPLE__
        #warning "MacOS X might not yet support OpenGL 3.3 (3.2 was the maximum for 10.8)"
    #endif
#else
	#ifdef __APPLE__
        glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR,  3);
        glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR,  2);
        glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #endif
#endif

#ifdef USE_DEBUG_CONTEXT
	glfwOpenWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

	if( !glfwOpenWindow( WIN_WIDTH, WIN_HEIGHT, 0,0,0,0, 0,0, GLFW_WINDOW ) )
	{
		fprintf(stderr, "Failed to open GLFW window\n");

		glfwTerminate();
		return EXIT_FAILURE;
	}

	glfwSetWindowTitle( BASE_TITLE );
	
	// Check OpenGL version
	// in case of GL 1.x or 2.x we have to check with glGetString:
    const GLubyte *version = glGetString(GL_VERSION);
    if ((version[0] == '2' || version[0] == '1') && version[1] == '.')
	{
        fprintf(stderr, "*** OpenGL version too low, need at least 3.2 for ARB_timer_query (%s)\n", version);
		return EXIT_FAILURE;
    }
    
    // getting the version with glGetInteger needs gl >= 3.0
    GLint major, minor;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	
	if(major == 3 && minor < 2)
	{
		fprintf(stderr, "*** OpenGL version too low, need at least 3.2 for ARB_timer_query (%d.%d)\n",major, minor);
		return EXIT_FAILURE;
	}
	printf("OpenGL version: %d.%d\n", major, minor);
    
	// Load the OpenGL implementation + available extensions
#ifdef __APPLE__
	glewExperimental = GL_TRUE; // needed for core profiles
#endif
    GLenum error = glewInit();
	if(error != GL_NO_ERROR)
	{
		fprintf(stderr, "Failed to initialize GLEW: error=%d\n", (int)error);
		glfwTerminate();
		return EXIT_FAILURE;
	}
	
	// Register a debug callback if supported by the driver
	if(GLEW_ARB_debug_output)
	{
		glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
		glDebugMessageCallbackARB((GLDEBUGPROCARB)&debugCallback, NULL);
	}

	// Setup a VAO for the whole time the program is executed
	GLuint id_vao;
	glGenVertexArrays(1, &id_vao);
	glBindVertexArray(id_vao);

	// Set the default OpenGL state
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Mouse and window size
	glfwGetMousePos(&mouse_x, &mouse_y);
	glfwGetWindowSize(&win_w, &win_h);

	glfwSetMouseButtonCallback(&onMouseClick);
	glfwSetKeyCallback(&onKey);

	// Initialize the 2D drawer
	if(!drawer2D.init(win_w, win_h))
	{
		fprintf(stderr, "*** FAILED initializing the Drawer2D\n");
		return EXIT_FAILURE;
	}

	// Initialize the profiler
	PROFILER_INIT(win_w, win_h, mouse_x, mouse_y);

	// Initialize the example scene
	if(!scene.init())
	{
		fprintf(stderr, "*** FAILED initializing the scene\n");
		return EXIT_FAILURE;
	}
	printf("Multithreaded update: %s\n", scene.isMultithreaded() ? "yes" : "no");

	// Enable vertical sync
	glfwSwapInterval( 1 );

	// Main loop
	double elapsed = 0.0;
	double t = glfwGetTime();
	double last_t = t;
	do
	{
		last_t = t;
		t = glfwGetTime();
		elapsed = t - last_t;

		PROFILER_SYNC_FRAME();
		PROFILER_PUSH_CPU_MARKER("Full frame", COLOR_GRAY);

		// Update mouse and window size
		prev_mouse_x = mouse_x;		prev_mouse_y = mouse_y;
		glfwGetMousePos( &mouse_x, &mouse_y);

		prev_win_w = win_w;			prev_win_h = win_h;
		win_h = win_h > 0 ? win_h : 1;

		glfwGetWindowSize(&win_w, &win_h);

		// Detect and notify changes in the mouse position or the window size
		if(prev_mouse_x != mouse_x || prev_mouse_y != mouse_y)
		{
			PROFILER_ON_MOUSE_POS(mouse_x, mouse_y);
		}
		if(prev_win_w != win_w || prev_win_h != win_h)
		{
			PROFILER_ON_RESIZE(win_w, win_h);
			drawer2D.onResize(win_w, win_h);
		}

		glViewport( 0, 0, win_w, win_h );

		// Clear color buffer to black
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

		glEnable(GL_DEPTH_TEST);

		PROFILER_PUSH_CPU_MARKER("Update scene", COLOR_GREEN);
		scene.update(elapsed, t);
		PROFILER_POP_CPU_MARKER();

		PROFILER_PUSH_CPU_MARKER("Draw scene", COLOR_RED);
		scene.draw(win_w, win_h);
		PROFILER_POP_CPU_MARKER();

		glDisable(GL_DEPTH_TEST);

		PROFILER_PUSH_GPU_MARKER("Draw profiler", COLOR_DARK_BLUE);
		PROFILER_DRAW();
		PROFILER_POP_GPU_MARKER();

		if(help_visible)
		{
			PROFILER_PUSH_CPU_MARKER("Draw help", COLOR_MAGENTA);
			drawHelp();
			PROFILER_POP_CPU_MARKER();
		}

		checkGLError();

		fpsCount(BASE_TITLE);

		// Swap buffers
		glfwSwapBuffers();

		PROFILER_POP_CPU_MARKER();

	} // Check if the ESC key was pressed or the window was closed
	while( glfwGetKey( GLFW_KEY_ESC ) != GLFW_PRESS && glfwGetWindowParam( GLFW_OPENED ) );

	done = true;

	scene.shut();

	PROFILER_SHUT();

	drawer2D.shut();

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	shutTimer();

	return EXIT_SUCCESS;
}

void GLFWCALL onMouseClick(int button, int action)
{
	if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
	{
		PROFILER_ON_LEFT_CLICK();
	}
}

void GLFWCALL onKey(int key, int action)
{
	if(action == GLFW_RELEASE)
	{
		switch(key)
		{
		case 'H':
			help_visible = !help_visible;
			break;
		case 'P':
			profiler.setVisible(!profiler.isVisible());
			break;
		case 'M':
			scene.setMultithreaded(!scene.isMultithreaded());
			printf("Multithreaded update: %s\n", scene.isMultithreaded() ? "yes" : "no");
			break;
		}
	}
}

void drawHelp()
{
	drawer2D.drawRect(Rect(0.1f, 0.1f, 0.8f, 0.8f), Color(0xC0, 0xC0, 0xC0), 0.5f);
	drawer2D.drawString(
		"Commands:\n"
		"[H]: display this help message\n"
		"[P]: profiler visiblity\n"
		"[M]: mono/multi threaded update\n"
		"[ESC]: quit\n"
		"click on the profiler to freeze it\n",
		0.12f, 1.0f-0.15f, COLOR_WHITE);
}
