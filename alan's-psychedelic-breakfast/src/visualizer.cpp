//-----------------------------------------------------------------------------
// name: Alan's Psychedelic Breakfast - visualizer.cpp
// desc: A visualizer for real-time audio
//
// author: Trijeet Mukhopadhyay (trijeetm@stanford.edu)
//   date: fall 2014
//   uses: RtAudio by Gary Scavone
//-----------------------------------------------------------------------------

#include "RtAudio.h"
#include "chuck_fft.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <thread>
using namespace std;

#ifdef __MACOSX_CORE__
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif




//-----------------------------------------------------------------------------
// function prototypes
//-----------------------------------------------------------------------------
void help();
void initGfx();
void idleFunc();
void displayFunc();
void reshapeFunc( GLsizei width, GLsizei height );
void keyboardFunc( unsigned char, int, int );
void mouseFunc( int button, int state, int x, int y );

// our datetype
#define SAMPLE float
// corresponding format for RtAudio
#define MY_FORMAT RTAUDIO_FLOAT32
// sample rate
#define MY_SRATE 44100
// number of channels
#define MY_CHANNELS 1
// for convenience
#define MY_PIE 3.14159265358979

// width and height
long g_width = 1024;
long g_height = 720;
long g_last_width = g_width;
long g_last_height = g_height;
// global buffer
SAMPLE * g_buffer = NULL;
long g_bufferSize;
// fft buffer
SAMPLE * g_fftBuf = NULL;
// freq domain buffer history
const long MAX_STATES = 61;
complex *g_FDBufHistory[MAX_STATES];
long g_nHistoryStates;
// window
SAMPLE * g_window = NULL;
long g_windowSize;

// global variables
GLboolean g_fullscreen = FALSE;
GLboolean g_toggleRave = FALSE;
GLboolean g_toggleBassPulses = TRUE;
GLboolean g_toggleMidPulses = TRUE;
GLboolean g_toggleTreblePulses = TRUE;
GLboolean g_flash = FALSE;
GLboolean g_toggleTDWaveform = TRUE;
GLboolean g_toggleFDWaveform = TRUE;
GLboolean g_allowAutoRave = FALSE;


struct Colorf {
    float red;
    float blue;
    float green;
};

struct SoundPulse {
    GLboolean on;
    float rad;
    Colorf col;
    float lineWidth;
    float transZ;
};

// rotation params for time domain waveform
float g_rad = 1.2;
float g_deltaRad = 0.1;
// rotation params for freq domain waveform
float g_rad2 = 2.6;
float g_deltaRad2 = 0.1;
// rotation params for time domain waveform lines
float g_zRotWaves = 0.5;    // slower
float g_zRotWaves2 = 0.5;    // faster
// rotation params for time domain waveform circle
float g_zRotWavesC = 0.5;
int g_flashFrame = 0;
int g_flashFR = 6;
// bass pulse governing params
const int MAX_BASS_PULSES = 40;
SoundPulse g_bassPulses[MAX_BASS_PULSES];
// bass pulse stagger params
int g_bassPulseCounter = 0;
const int BASS_PULSE_STAGGER = 200;
int g_bassPulseIndex = 0;
// mid pulse governing params
const int MAX_MID_PULSES = 50;
SoundPulse g_midPulses[MAX_MID_PULSES];
// mid pulse stagger params
int g_midPulseCounter = 0;
const int MID_PULSE_STAGGER = 400;
int g_midPulseIndex = 0;


//-----------------------------------------------------------------------------
// name: callme()
// desc: audio callback
//-----------------------------------------------------------------------------
int callme( void * outputBuffer, void * inputBuffer, unsigned int numFrames,
            double streamTime, RtAudioStreamStatus status, void * data )
{
    // cast!
    SAMPLE * input = (SAMPLE *)inputBuffer;
    SAMPLE * output = (SAMPLE *)outputBuffer;
    
    // fill
    for( int i = 0; i < numFrames; i++ )
    {
        // assume mono
        g_buffer[i] = input[i];
        // zero output
        output[i] = 0;
    }
    
    return 0;
}




//-----------------------------------------------------------------------------
// name: main()
// desc: entry point
//-----------------------------------------------------------------------------
int main( int argc, char ** argv )
{
    // seed RNG
    srand(time(NULL));
    // instantiate RtAudio object
    RtAudio audio;
    // variables
    unsigned int bufferBytes = 0;
    // frame size
    unsigned int bufferFrames = 1024;
    
    // check for audio devices
    if( audio.getDeviceCount() < 1 )
    {
        // nopes
        cout << "no audio devices found!" << endl;
        exit( 1 );
    }
    
    // initialize GLUT
    glutInit( &argc, argv );
    // init gfx
    initGfx();
    
    // let RtAudio print messages to stderr.
    audio.showWarnings( true );
    
    // set input and output parameters
    RtAudio::StreamParameters iParams, oParams;
    iParams.deviceId = audio.getDefaultInputDevice();
    iParams.nChannels = MY_CHANNELS;
    iParams.firstChannel = 0;
    oParams.deviceId = audio.getDefaultOutputDevice();
    oParams.nChannels = MY_CHANNELS;
    oParams.firstChannel = 0;
    
    // create stream options
    RtAudio::StreamOptions options;
    
    // go for it
    try {
        // open a stream
        audio.openStream( &oParams, &iParams, MY_FORMAT, MY_SRATE, &bufferFrames, &callme, (void *)&bufferBytes, &options );
    }
    catch( RtError& e )
    {
        // error!
        cout << e.getMessage() << endl;
        exit( 1 );
    }
    
    // compute
    bufferBytes = bufferFrames * MY_CHANNELS * sizeof(SAMPLE);
    // allocate global buffer
    g_bufferSize = bufferFrames;
    g_buffer = new SAMPLE[g_bufferSize];
    g_fftBuf = new SAMPLE[g_bufferSize];
    memset( g_buffer, 0, sizeof(SAMPLE) * g_bufferSize );
    memset( g_fftBuf, 0, sizeof(SAMPLE) * g_bufferSize );
    
    // allocate buffer to hold window
    g_windowSize = bufferFrames;
    g_window = new SAMPLE[g_windowSize];
    // generate the window
    hanning( g_window, g_windowSize );
    
    // init bass pulses
    for (int i = 0; i < MAX_BASS_PULSES; i++) {
        g_bassPulses[i].rad = 0;
        g_bassPulses[i].on = FALSE;
        g_bassPulses[i].col.red = 0.5;
        g_bassPulses[i].col.green = 0.5;
        g_bassPulses[i].col.blue = 1.0;
        g_bassPulses[i].lineWidth = 0;
        g_bassPulses[i].transZ = 0;
    }
    // init mid pulses
    for (int i = 0; i < MAX_MID_PULSES; i++) {
        g_midPulses[i].rad = 0;
        g_midPulses[i].on = FALSE;
        g_midPulses[i].col.red = 0.5;
        g_midPulses[i].col.green = 0.5;
        g_midPulses[i].col.blue = 1.0;
        g_midPulses[i].lineWidth = 0;
        g_midPulses[i].transZ = 0;
    }

    // array of complex buffers to store history
    const long COMP_BUF_SIZE = g_bufferSize / 2;
    // g_FDBufHistory = new complex[MAX_STATES][COMP_BUF_SIZE];
    for (int i = 0; i < MAX_STATES; i++) {
        g_FDBufHistory[i] = new complex[COMP_BUF_SIZE];
        memset(g_FDBufHistory[i], 0, sizeof(complex) * (COMP_BUF_SIZE));
    }

    // print help
    help();
    
    // go for it
    try {
        // start stream
        audio.startStream();
        
        // let GLUT handle the current thread from here
        glutMainLoop();
        
        // stop the stream.
        audio.stopStream();
    }
    catch( RtError& e )
    {
        // print error message
        cout << e.getMessage() << endl;
        goto cleanup;
    }
    
cleanup:
    // close if open
    if( audio.isStreamOpen() )
        audio.closeStream();
    
    // done
    return 0;
}




//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void initGfx()
{
    // double buffer, use rgb color, enable depth buffer
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
    // initialize the window size
    glutInitWindowSize( g_width, g_height );
    // set the window postion
    glutInitWindowPosition( 100, 100 );
    // create the window
    glutCreateWindow("Alan's Psychedelic Breakfast");
    
    // set the idle function - called when idleFunc
    glutIdleFunc( idleFunc );
    // set the display function - called when redrawing
    glutDisplayFunc( displayFunc );
    // set the reshape function - called when client area changes
    glutReshapeFunc( reshapeFunc );
    // set the keyboard function - called on keyboard events
    glutKeyboardFunc( keyboardFunc );
    // set the mouse function - called on mouse stuff
    glutMouseFunc( mouseFunc );
    
    // set clear color
    glClearColor( 0, 0, 0, 1 );
    // enable color material
    glEnable( GL_COLOR_MATERIAL );
    // enable depth test
    glEnable( GL_DEPTH_TEST );
    // enable blending
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
}




//-----------------------------------------------------------------------------
// Name: reshapeFunc( )
// Desc: called when window size changes
//-----------------------------------------------------------------------------
void reshapeFunc( GLsizei w, GLsizei h )
{
    // save the new window size
    g_width = w; g_height = h;
    // map the view port to the client area
    glViewport( 0, 0, w, h );
    // set the matrix mode to project
    glMatrixMode( GL_PROJECTION );
    // load the identity matrix
    glLoadIdentity( );
    // create the viewing frustum
    gluPerspective( 45.0, (GLfloat) w / (GLfloat) h, 1.0, 300.0 );
    // set the matrix mode to modelview
    glMatrixMode( GL_MODELVIEW );
    // load the identity matrix
    glLoadIdentity( );
    // position the view point
    gluLookAt( 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f );
}




//-----------------------------------------------------------------------------
// Name: help( )
// Desc: print usage
//-----------------------------------------------------------------------------
void help()
{
    cerr << "----------------------------------------------------" << endl;
    cerr << "Alan's Psychedelic Breakfast" << endl;
    cerr << "Trijeet Mukhopadhyay" << endl;
    cerr << "http://ccrma.stanford.edu/~trijeetm/alan's-psychedelic-breakfast" << endl;
    cerr << "----------------------------------------------------" << endl;
    cerr << "'h' - print this help message" << endl;
    cerr << "'s' - toggle fullscreen" << endl;
    cerr << "'q' - quit visualization" << endl;
    cerr << "'1' - toggle time domain waveforms" << endl;
    cerr << "'2' - toggle frequency domain waveforms" << endl;
    cerr << "'b' - toggle bass pulses" << endl;
    cerr << "'m' - toggle mid pulses" << endl;
    cerr << "'<space bar>' - toggle rave (flashing background) mode" << endl;
    cerr << "'r' - toggle auto-rave mode" << endl;
    cerr << "----------------------------------------------------" << endl;
}




//-----------------------------------------------------------------------------
// Name: keyboardFunc( )
// Desc: key event
//-----------------------------------------------------------------------------
void keyboardFunc( unsigned char key, int x, int y )
{
    switch( key )
    {
        case 'q': // quit
            exit(1);
            break;
            
        case 'h': // print help
            help();
            break;
            
        case 's': // toggle fullscreen
        {
            // check fullscreen
            if( !g_fullscreen )
            {
                g_last_width = g_width;
                g_last_height = g_height;
                glutFullScreen();
            }
            else
                glutReshapeWindow( g_last_width, g_last_height );
            
            // toggle variable value
            g_fullscreen = !g_fullscreen;
        }
        break;
        case ' ': // toggle rave mode
            g_toggleRave = !g_toggleRave;
        break;
        case '1': // toggle time domain waveform
            g_toggleTDWaveform = !g_toggleTDWaveform;
        break;
        case '2': // toggle freq domain waveform
            g_toggleFDWaveform = !g_toggleFDWaveform;
        break;
        case 'b': // toggle bass pulses
            g_toggleBassPulses = !g_toggleBassPulses;
        break;
        case 'm': // toggle mid pulses
            g_toggleMidPulses = !g_toggleMidPulses;
        break;
        case 'r': // toggle auto rave
            g_allowAutoRave = !g_allowAutoRave;
        break;
    }
    
    // trigger redraw
    glutPostRedisplay( );
}




//-----------------------------------------------------------------------------
// Name: mouseFunc( )
// Desc: handles mouse stuff
//-----------------------------------------------------------------------------
void mouseFunc( int button, int state, int x, int y )
{
    if( button == GLUT_LEFT_BUTTON )
    {
        // when left mouse button is down
        if( state == GLUT_DOWN )
        {
        }
        else
        {
        }
    }
    else if ( button == GLUT_RIGHT_BUTTON )
    {
        // when right mouse button down
        if( state == GLUT_DOWN )
        {
        }
        else
        {
        }
    }
    else
    {
    }
    
    glutPostRedisplay( );
}




//-----------------------------------------------------------------------------
// Name: idleFunc( )
// Desc: callback from GLUT
//-----------------------------------------------------------------------------
void idleFunc( )
{
    // render the scene
    glutPostRedisplay( );
}

const float DEG2RAD = 3.14159 / 180;
 
void drawCircle(float radius) {
    glBegin(GL_LINE_LOOP);
    for (int i = 180; i < 361; i++) {
        float degInRad = i*DEG2RAD;
        glVertex2f(cos(degInRad) * radius, sin(degInRad) * radius);
    }
    glEnd();
}

void drawSemiCircle(float radius) {
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 181; i++) {
        float degInRad = i*DEG2RAD;
        glVertex2f(cos(degInRad) * radius, sin(degInRad) * radius);
    }
    glEnd();
}

Colorf g_centralCol;
int g_centralColTracker = 0;
Colorf g_secondaryCol;
int g_secondaryColTracker = 0;

GLboolean g_forceRave = false;

//-----------------------------------------------------------------------------
// Name: displayFunc( )
// Desc: callback function invoked to draw the client area
//-----------------------------------------------------------------------------
void displayFunc( )
{
    // calculate central color
    if (g_centralColTracker % 6 == 0) {
        g_centralCol.red = (rand() % 6 / 100.00) + 0.94;
        g_centralCol.green = (rand() % 5 / 100.00) + 0.45;
        g_centralCol.blue = (rand() % 5 / 100.00) + 0.01;
        
        g_secondaryCol.red = 1.0;
        g_secondaryCol.green = 1.0;;
        g_secondaryCol.blue = 1.0;;
        g_secondaryColTracker++;
    }
    g_centralColTracker++;


    // calculate average value of TD waveform
    float avgTDWaveformVal = 0;
    for (int i = 0; i < g_bufferSize; i++) {
        avgTDWaveformVal += ::fabs(g_buffer[i]);
    }
    avgTDWaveformVal /= g_bufferSize;

    // cerr << "avgTDWaveformVal = " << avgTDWaveformVal << endl;

    g_flashFR = floor(pow(5000 * avgTDWaveformVal, 0.5) * 2);
    // cerr << g_flashFR << endl;

    if (g_flashFrame > g_flashFR) {
        g_flashFrame = 0;
        g_flash = !g_flash;
    }
    g_flashFrame++;

    if (avgTDWaveformVal > 0.015)
        g_forceRave = true;
    else 
        g_forceRave = FALSE;

    // local state
    static GLfloat zrot = 0.0f, c = 0.0f;
    
    // clear the color and depth buffers
    if (g_toggleRave || (g_forceRave && g_allowAutoRave)) {
        if (g_flash)
            glClearColor(((rand() % 100) / 100.00), ((rand() % 100) / 100.00), ((rand() % 100) / 100.00), 1.0);
        else 
            glClearColor(0.0, 0.0, 0.0, 1.0);
    }
    else    
        glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    
    // line width
    glLineWidth( 6.0 );
    // color
    glColor3f(g_centralCol.red, g_centralCol.green, g_centralCol.blue);

    if (g_toggleTDWaveform) {
        // time domain waveform circular
        glPushMatrix();
            glRotatef(g_zRotWavesC, 0, 0, 1);
            glBegin(GL_POLYGON);
                for (int i = 0; i < 360; i++)
                {
                    float degInRad = i * DEG2RAD;
                    glVertex2f(cos(degInRad) * (g_rad + (1 * g_buffer[i + 360])), sin(degInRad) * (g_rad + (1 * g_buffer[i + 360])));
                }
                // pulsate the circle
                    if (g_rad >= 1.4) {
                        g_deltaRad = -(pow(avgTDWaveformVal, 0.4) / 25.0);
                        // g_deltaRad = -0.0075;
                    }
                    else if (g_rad <= 1.2) {
                        g_deltaRad = (pow(avgTDWaveformVal, 0.4) / 25.0);
                        // g_deltaRad = 0.005;
                    }
                    g_rad += g_deltaRad;
            glEnd();
            g_zRotWavesC += 0.3;
        glPopMatrix();

        // line width
        glLineWidth( 1.0 );

        // apply window to buf
        apply_window( g_buffer, g_window, g_windowSize );

        // for rotating the time domain waveforms
        glPushMatrix();
            glRotatef(g_zRotWaves, 0, 0, 1);
            // draw time domain waveform line plot
            // define a starting point
            GLfloat x = -8;
            // compute increment
            GLfloat xinc = ::fabs(x*2 / g_bufferSize);
            // color
            // glColor3f(1, 0.5, 0.5);
            glColor3f(((rand() % 100) / 100.00), ((rand() % 100) / 100.00), ((rand() % 100) / 100.00));
            // save transformation state
            glPushMatrix();
                // translate
                glTranslatef( 0, 0, 0 );
                // start primitive
                glBegin( GL_LINE_STRIP );
                    // loop over buffer
                    for( int i = 0; i < g_bufferSize; i++ )
                    {
                        // plot
                        glVertex2f( x, ((10 * g_buffer[i])) );
                        // increment x
                        x += xinc;
                    }
                // end primitive
                glEnd();
            // pop
            glPopMatrix();

            x = -8;
            // save transformation state
            glPushMatrix();
                // translate
                glRotatef(90, 0, 0, 1);
                glTranslatef( 0, 0, 0 );
                // start primitive
                glBegin( GL_LINE_STRIP );
                    // loop over buffer
                    for( int i = 0; i < g_bufferSize; i++ )
                    {
                        // plot
                        glVertex2f( x, ((10 * g_buffer[i])) );
                        // increment x
                        x += xinc;
                    }
                // end primitive
                glEnd();
            // pop
            glPopMatrix();
            // g_zRotWaves += ((rand() % 100) / 100.00) + 1;
            g_zRotWaves += pow((avgTDWaveformVal * 100.00), 0.15) * 2;
        glPopMatrix();

        glLineWidth(2.5);

        // for faster rotating the time domain waveforms
        glPushMatrix();
            glRotatef(g_zRotWaves2, 0, 0, 1);
            // draw time domain waveform line plot
            // define a starting point
            x = -8;
            // random color
            glColor3f(((rand() % 100) / 100.00), ((rand() % 100) / 100.00), ((rand() % 100) / 100.00));
            // save transformation state
            glPushMatrix();
                // translate
                glTranslatef( 0, 0, 0 );
                // start primitive
                glBegin( GL_LINE_STRIP );
                    // loop over buffer
                    for( int i = 0; i < g_bufferSize; i++ )
                    {
                        // plot
                        glVertex2f( x, ((10 * g_buffer[i])) );
                        // increment x
                        x += xinc;
                    }
                // end primitive
                glEnd();
            // pop
            glPopMatrix();

            x = -8;
            // save transformation state
            glPushMatrix();
                // translate
                glRotatef(90, 0, 0, 1);
                glTranslatef( 0, 0, 0 );
                // start primitive
                glBegin( GL_LINE_STRIP );
                    // loop over buffer
                    for( int i = 0; i < g_bufferSize; i++ )
                    {
                        // plot
                        glVertex2f( x, ((10 * g_buffer[i])) );
                        // increment x
                        x += xinc;
                    }
                // end primitive
                glEnd();
            // pop
            glPopMatrix();
            // g_zRotWaves2 -= ((rand() % 400) / 100.00) + 2;
            g_zRotWaves2 -= pow((avgTDWaveformVal * 100.00), 0.15) * 3;
        glPopMatrix();


        // horizon line

        x = -7;
        // save transformation state
        glPushMatrix();
            // translate
            glTranslatef( 0, 0, 0 );
            glLineWidth(12.0);
            glColor3f(g_secondaryCol.red, g_secondaryCol.green, g_secondaryCol.blue);
            // start primitive
            glBegin( GL_LINE_STRIP );
                // loop over buffer
                for( int i = 0; i < g_bufferSize; i++ )
                {
                    // plot
                    glVertex2f( x, g_buffer[i] );
                    // increment x
                    x += xinc;
                }
            // end primitive
            glEnd();
        // pop
        glPopMatrix();
    }
    else {
        // apply window to buf
        apply_window( g_buffer, g_window, g_windowSize );
    }
    
    // copy into the fft buf
    memcpy( g_fftBuf, g_buffer, sizeof(SAMPLE) * g_bufferSize );
    
    // take forward FFT (time domain signal -> frequency domain signal)
    rfft( g_fftBuf, g_windowSize / 2, FFT_FORWARD );
    // cast the result to a buffer of complex values (re,im)
    complex * cbuf = (complex *)g_fftBuf;

// BASS PULSES
    if (g_toggleBassPulses) {
        // check for bass pulses
        for (int i = 0; i < ((g_windowSize / 2) / 100) * 4; i++) {
            if (cmp_abs(cbuf[i]) > 0.001) {
                g_bassPulseCounter = (g_bassPulseCounter + 1) % BASS_PULSE_STAGGER;
                if (g_bassPulseCounter == 0) {
                    // cerr << cmp_abs(cbuf[i]) << endl;
                    int g_bassPulseLastIndex = ((g_bassPulseIndex == 0) ? MAX_BASS_PULSES : g_bassPulseIndex) - 1;
                    for (int j = g_bassPulseIndex; j != g_bassPulseLastIndex; j = (j + 1) % MAX_BASS_PULSES) {
                        if (j == g_bassPulseIndex) {
                            // cerr << ". ";
                            g_bassPulses[j].on = true;
                            g_bassPulses[j].rad = g_rad * 2;
                            g_bassPulses[j].col.green = (rand() % 30 / 100.00) + 0.2;
                            g_bassPulses[j].col.blue = (rand() % 10 / 100.00) + 0.9;
                            g_bassPulses[j].col.red = (rand() % 30 / 100.00) + 0.3;
                            g_bassPulses[j].lineWidth = 30.0;
                            g_bassPulses[j].transZ = -0.0000000001;
                        }
                    }
                    g_bassPulseIndex = (g_bassPulseIndex + 1) % MAX_BASS_PULSES;
                }
            }
        }


        // draw bass pulses
        for (int i = 0; i < MAX_BASS_PULSES; i++) {
            // cerr << "index = " << i << endl
                // << "\t" << (g_bassPulses[i].on ? "on" : "off") << ":\t" << g_bassPulses[i].rad << endl;
            glLineWidth(5.0);
            glColor3f(0.5, 0.5, 1.0);
            if (g_bassPulses[i].on) {
                if (g_bassPulses[i].rad < 10) 
                g_bassPulses[i].rad = g_bassPulses[i].rad + 0.075;
                g_bassPulses[i].col.red -= (g_bassPulses[i].col.red * 0.005);
                g_bassPulses[i].col.green -= (g_bassPulses[i].col.green * 0.005);
                g_bassPulses[i].col.blue -= (g_bassPulses[i].col.blue * 0.005);
                g_bassPulses[i].lineWidth -= 0.01;
                g_bassPulses[i].transZ -= 0.03;
                glPushMatrix();
                    glColor3f(g_bassPulses[i].col.red, g_bassPulses[i].col.green, g_bassPulses[i].col.blue);
                    glLineWidth(g_bassPulses[i].lineWidth);
                    glTranslatef(0, 0, g_bassPulses[i].transZ);
                    if (g_bassPulses[i].col.red && g_bassPulses[i].col.green && g_bassPulses[i].col.blue)
                        drawCircle(g_bassPulses[i].rad);
                glPopMatrix();
            }
        }
    }
    

//  MID PULSES
    if (g_toggleMidPulses) {
        // check for mid pulses
        for (int i = 1 + ((g_windowSize / 2) / 100) * 4; i < ((g_windowSize / 2) / 100) * 80; i++) {
            if (cmp_abs(cbuf[i]) > 0.0004) {
                g_midPulseCounter = (g_midPulseCounter + 1) % MID_PULSE_STAGGER;
                if (g_midPulseCounter == 0) {
                    int g_midPulseLastIndex = ((g_midPulseIndex == 0) ? MAX_MID_PULSES : g_midPulseIndex) - 1;
                    for (int j = g_midPulseIndex; j != g_midPulseLastIndex; j = (j + 1) % MAX_MID_PULSES) {
                        if (j == g_midPulseIndex) {
                            // cerr << ". ";
                            g_midPulses[j].on = true;
                            g_midPulses[j].rad = 0.25;
                            g_midPulses[j].col.green = (rand() % 30 / 100.00) + 0.2;
                            g_midPulses[j].col.red = (rand() % 10 / 100.00) + 0.9;
                            g_midPulses[j].col.blue = (rand() % 30 / 100.00) + 0.3;
                            g_midPulses[j].lineWidth = 5.0;
                            g_midPulses[j].transZ = -0.000000000000;
                        }
                    }
                    g_midPulseIndex = (g_midPulseIndex + 1) % MAX_MID_PULSES;
                }
            }
        }

        // draw mid pulses
        for (int i = 0; i < MAX_MID_PULSES; i++) {
            // cerr << "index = " << i << endl
                // << "\t" << (g_midPulses[i].on ? "on" : "off") << ":\t" << g_midPulses[i].rad << endl;
            glLineWidth(5.0);
            glColor3f(0.5, 0.5, 1.0);
            if (g_midPulses[i].on) {
                if (g_midPulses[i].rad < 10)
                    g_midPulses[i].rad = g_midPulses[i].rad + 0.075;
                g_midPulses[i].col.red -= (g_midPulses[i].col.red * 0.005);
                g_midPulses[i].col.green -= (g_midPulses[i].col.green * 0.005);
                g_midPulses[i].col.blue -= (g_midPulses[i].col.blue * 0.005);
                g_midPulses[i].lineWidth -= 0.01;
                g_midPulses[i].transZ -= 0.04;
                glPushMatrix();
                    glColor3f(g_midPulses[i].col.red, g_midPulses[i].col.green, g_midPulses[i].col.blue);
                    glLineWidth(g_midPulses[i].lineWidth);
                    glTranslatef(0, 0, g_midPulses[i].transZ);
                    if (g_midPulses[i].col.red && g_midPulses[i].col.green && g_midPulses[i].col.blue)
                        drawSemiCircle(g_midPulses[i].rad);
                glPopMatrix();
            }
        }
    }
         
    if (g_toggleFDWaveform) {
        // save frequency domain buffer state
        // cerr << endl << endl << "Shifting buffer history by one" << endl;
        for (int i = g_nHistoryStates - 1; i > 0; i--) {
            memcpy(g_FDBufHistory[i], g_FDBufHistory[i - 1], sizeof(complex) * (g_windowSize / 2));
        }
        if (g_nHistoryStates < MAX_STATES) 
            g_nHistoryStates++;
        // cerr << "Copying current cBuf into history" << endl;
        memcpy(g_FDBufHistory[0], cbuf, sizeof(complex) * (g_windowSize / 2));

        
        // Drawing freq domain plot
        glPushMatrix();
            glLineWidth(2);
            // define a starting point
            // set color to green
            // glColor3f(1, 1, 1);
            glColor3f(g_secondaryCol.red, g_secondaryCol.green, g_secondaryCol.blue);
            // reset x
            float x = -g_rad * 2;
            // compute increment
            float xinc = ::fabs(1.2 * x / (2 * (g_windowSize / 2)));
            // glRotatef(-25, 1, 0, 0);
            glTranslatef(0, 0, 0.00001);
            // glColor3f(((rand() % 100) / 100.00), ((rand() % 100) / 100.00), ((rand() % 100) / 100.00));
            // for (int i = 0; i < 1; i++) {
            for (int i = 0; i < g_nHistoryStates; i++) {
                glPushMatrix();
                    // random color
                        // if (i > 0)
                        //     glColor3f(((rand() % 90) / 100.00), ((rand() % 90) / 100.00), ((rand() % 90) / 100.00));
                    glRotatef(i * 3, 0, 0, 1);
                    glPushMatrix();
                        // translate
                        // glTranslatef(0, -1, -(i / 2.0));
                        // start primitive
                        glBegin( GL_LINE_STRIP );
                            x = -g_rad * 2.2;
                            // shoot up scaling by percentage
                            float scalingFactor = 20;
                            if (rand() % 100 > 90) 
                                scalingFactor = 13;
                            else 
                                scalingFactor = 7;
                            // loop over buffer to draw spectrum
                            for(int j = 0; j < ((g_windowSize / 2) / 100) * 100; j++)
                            // for(int j = 1 + ((g_windowSize / 2) / 100) * 4; j < ((g_windowSize / 2) / 100) * 100; j++)
                            {
                                // plot the magnitude,
                                // with scaling, and also "compression" via pow(...)
                                glVertex2f( x, scalingFactor * pow( cmp_abs(g_FDBufHistory[i][j]), .4 ) );
                                // increment x
                                x += xinc;
                            }
                        // end primitive
                        glEnd();
                    glPopMatrix();
                // restore transformations
                glPopMatrix();
            }
        glPopMatrix();
    }
    

    // freq domain waveform circularm
        // glLineWidth(1.5);
        // glColor3f(0.5, 1.0, 0.5);
        // glBegin(GL_LINE_LOOP);
        //     for (int i=0; i < 360; i++)
        //     {
        //         float degInRad = i*DEG2RAD;
        //         glVertex2f(cos(degInRad) * ((g_rad2 * (5 * cmp_abs(cbuf[i]))) + 3), sin(degInRad) * ((g_rad2 * (5 * cmp_abs(cbuf[i]))) + 3));
        //     }
        //     if (g_rad2 > 2.6) {
        //         g_deltaRad2 = -0.015;
        //     }
        //     else if (g_rad2 < 2.0) {
        //         g_deltaRad2 = 0.01;
        //     }
        //     g_rad2 += g_deltaRad2;
        // glEnd();
    
    // flush!
    glFlush( );
    // swap the double buffer
    glutSwapBuffers( );
}
