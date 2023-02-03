/*
 * Elementary simulation using GLFW + OpenGL for display modified for use in PlantSim by Finley Webb
 * Original code by Francois J Nedelec, Cambridge University, 13 Nov 2021, 11 Oct 2022
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define DEBUG true
#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <vector>

#include "random.h"
#include "param.h"
#include "object.h"
#include "polish.h"
#include "Clarkson-Delaunay.cpp"  /// this is slightly odd, would be better to compile them seperately and link together (don't know how to do that lol)


///-----------------------------------------------------------------------------


/// evolves system, stepping points forward and accelerating velocity
static void animate(){
    realTime += delta;
    for ( int i = 0; i < nbo; ++i ) {
        pointsArray[i].step();
    }
}

/// creates an array of xy co-ords for the delaunay triangulation function, then execute it
void create_triangles_list(){
    float xyValuesArray[nbo][2];
    for ( int i = 0; i < nbo; i++){
        xyValuesArray[i][0] = pointsArray[i].x;
        xyValuesArray[i][1] = pointsArray[i].y;
    }

    numTriangleVertices = 0;
    triangleIndexList = BuildTriangleIndexList((void*)xyValuesArray, (float)1.0, nbo, (int)2, (int)1, &numTriangleVertices); /// having issues calling the delaunay tessleation function here

#if DEBUG
    printf("\nThere are %d points moving around", nbo);
    printf("\nThe number of vertices defined by numTriangleVertices is %d", numTriangleVertices);
    printf("\ntriangleIndexList contains the values: ");
    for (int i = 0; i < numTriangleVertices; i++)
        printf("%u, ", triangleIndexList[i]);
#endif ///DEBUG


}

/// checks through the pointsConnected array to see if secondary point is present
bool noDuplicateCheck(int indexValueToCheck, int arrayToCheck[], int max){
    static bool unique = true;

    for (int i = 0; i <= max; i++) {  /// going up to total instead of over all the array is faster
        if (arrayToCheck[i] == indexValueToCheck) {
            unique = false;
        }
    }

    if ((unique = true)){
        return true;
    }
    else{
        return false;
    }
}
/// TODO current algorithm grows with N squared, as you have N points which you scan over list of length N
/// TODO add an alogirhtm to create an adjacency matrix, use that to calculate forces between neighbours



/// repels/attracts points to each other dependent on relative displacement
void calculateSpringForces(){
    for(int i = 0; i < nbo; i++) { ///for each primary point in pointsArray (iterates through each point using i)

        int pointsConnected[MAX]; /// create an array for the neighbours of a primary point
        int total = 0; /// is a pointer for the pointsConnected array
        pointsArray[i].xSpringForce = 0; /// set spring forces to 0
        pointsArray[i].ySpringForce = 0;

        for(int j = 0; j < numTriangleVertices; j +=3){ /// we step through triangleIndexList in 3's
            if((triangleIndexList[j] == i) ||
               (triangleIndexList[j+1] == i) ||
               (triangleIndexList[j+2] == i)){  /// iterates through each triangle to check if any of the vertices are the primary point
                for(int k = 0; k < 3; k++){  /// triangle iterated through using k
                    /// below checks the secondary point isn't the same as the primary point and has not been referenced before
                    if((triangleIndexList[k+j] != i) and (noDuplicateCheck(triangleIndexList[j+k], pointsConnected, total) == true)){
                        pointsConnected[total] = triangleIndexList[j+k];  /// adds the connected points to the array
                        total++;  /// increments the pointer of the pointsConnected Array
                    }
                }
            }
        }

        /// now we've gotten all the connected points we need to change the velocity of each central point in turn
        for (int l = 0; l < total; l++) {
            /// find the magnitude of distance between the neighbouring point and the central point
            double magnitudeOfDistance = sqrt((pow((pointsArray[pointsConnected[l]].x) - (pointsArray[i].x), 2)
                                                + pow((pointsArray[pointsConnected[l]].y) - (pointsArray[i].y), 2)));
            double deltaMagnitude = magnitudeOfDistance - repulsionRadius;
            if ((deltaMagnitude > 0)){
            /// aka point exists outside of the repulsion radius of neighbour it is attracted
                pointsArray[i].xSpringForce += (pointsArray[pointsConnected[l]].x - (pointsArray[i].x))
                                            * (deltaMagnitude/magnitudeOfDistance) * pointsArray[i].extendedHooks;  /// deltaMag/Mag is needed to scale the x component to only that outside the radius of equilibrium
                pointsArray[i].ySpringForce += (pointsArray[pointsConnected[l]].y - (pointsArray[i].y))
                                            * (deltaMagnitude/magnitudeOfDistance) * pointsArray[i].extendedHooks;
            }
            else if ((deltaMagnitude < 0)){
            /// aka point exists within the radius of the neighbouring point and is repelled
                pointsArray[i].xSpringForce -= ((pointsArray[pointsConnected[l]].x) - (pointsArray[i].x)) * pointsArray[i].compressedHooks;
                pointsArray[i].ySpringForce -= ((pointsArray[pointsConnected[l]].y) - (pointsArray[i].y)) * pointsArray[i].compressedHooks;
            }
        }
    }
}

void iterateDisplace(){
    for(int i = 0; i<nbo; i++){
        pointsArray[i].step();
    }
}

/// draws the square in the window that contains the poinst
void drawSquare(float w, float h){
    glColor3f(0.5, 0.5, 0.5);
    glLineWidth(3);
    glBegin(GL_LINE_LOOP);
    glVertex2f(-w, -h);
    glVertex2f( w, -h);
    glVertex2f( w,  h);
    glVertex2f(-w,  h);
    glEnd();
}

/// draws the points as single points in the window
static void drawPoints(){
    glClear(GL_COLOR_BUFFER_BIT);
    
    // draw system's edges
    drawSquare(xBound, yBound);
    
    // draw particles as points:
    glPointSize(6);
    glBegin(GL_POINTS);
    for ( size_t i = 0; i < nbo; ++i )
        pointsArray[i].displayYellow();
    glEnd();
    
    //printf("draw @ %f\n", realTime);
    glFlush();
}

/// connects 3 consecutive points into triangles, if using the indexed list of points created
/// by the delaunay triangulation it should produce the triangulation
static void drawTrianglesAndPoints(){
    glClear(GL_COLOR_BUFFER_BIT);

    /// draw system's edges
    drawSquare(xBound, yBound);

    /// draw particles as Triangles:
    for(int i = 0; i < numTriangleVertices; i += 3) { /// iterates through 3 points at a time, needed as it loops back to the start of each polygon
        glBegin(GL_LINE_LOOP);
        for (int j = 0; j <= 2; ++j) {
            pointsArray[triangleIndexList[i + j]].displayYellow();

        }
glEnd();
        /// overlay points on top
        for(int k = 0; k < nbo; k += 1) {
            glPointSize(10);
            glBegin(GL_POINTS);
            pointsArray[k].displayWhite();
            glEnd();
        }
    }

    printf("draw @ %f\n", realTime);
    glFlush();
}

static void drawTriangles(){
    glClear(GL_COLOR_BUFFER_BIT);

    // draw system's edges
    drawSquare(xBound, yBound);

    // draw particles as Triangles:
    for(int i = 0; i < numTriangleVertices; i += 3) { /// iterates through 3 points at a time, needed as it loops back to the start of each polygon
        glBegin(GL_LINE_LOOP);
        for (int j = 0; j <= 2; ++j) {
            pointsArray[triangleIndexList[i + j]].displayYellow();

        }
        glEnd();
    }

    printf("draw @ %f\n", realTime);
    glFlush();
}

/// some more graphics stuff
/* change view angle, exit upon ESC */
void key(GLFWwindow* win, int k, int s, int action, int mods){
    if ( action != GLFW_PRESS )
        return;
    
    switch (k)
    {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(win, GLFW_TRUE);
            break;
        case GLFW_KEY_UP:
            break;
        case GLFW_KEY_DOWN:
            break;
        case GLFW_KEY_LEFT:
            break;
        case GLFW_KEY_RIGHT:
            break;
        default:
            return;
    }
}

/* change window size, adjust display to maintain isometric axes */
void reshape(GLFWwindow* win, int W, int H){
    glfwGetWindowSize(win, &winW, &winH);
    //printf("window size %i %i buffer : %i %i\n", winW, winH, W, H);

    pixel = 2 * std::min(xBound/winW, yBound/winH);

    glViewport(0, 0, W, H);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // buffer size:
    double mag = std::min(xBound/W, yBound/H);
    glOrtho(-mag * W, mag * W, -mag * H, mag * H, -1, 1);
    
    //printf("window size %f %f\n", midW, midH);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

/* program & OpenGL initialization */
static void init(GLFWwindow* win){
    // Set GLFW callback functions
    glfwSetFramebufferSizeCallback(win, reshape);
    glfwSetKeyCallback(win, key);
    
    glfwMakeContextCurrent(win);
    gladLoadGL(glfwGetProcAddress);
    glfwSwapInterval(1);
    
    int W = winW, H = winH;
    glfwGetFramebufferSize(win, &W, &H);
    reshape(win, W, H);

    // Init OpenGL rendering
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_POINT_SMOOTH);
    glDisable(GL_DEPTH_TEST);
}


/* program entry */
/// argc is the number of arguements, argv    y = yBound * srand(); is pointer to array of strings
int main(int argc, char *argv[]){
    for ( int i=1; i<argc; ++i ) {  /// iterates through arguements
       if ( 0 == readOption(argv[i]) )
           printf("Argument '%s' was ignored\n", argv[i]);
    }
    printf("I am here \n");
    printf("Size of point object is %ld \n", sizeof(Point));
    limitNbo();
    
    if ( !glfwInit() )
    {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return EXIT_FAILURE;
    }
    glfwSetErrorCallback(error);

    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    //glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    //glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
    
    GLFWwindow* win = glfwCreateWindow(winW, winH, "FRAP", NULL, NULL);
    if (!win)
    {
        fprintf(stderr, "Failed to open GLFW window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    init(win);


    double next = 0;
    while( !glfwWindowShouldClose(win) )
    {
        static int interationNumber = 1;
        double now = glfwGetTime();
        if ( now > next)
        {
            interationNumber++;
            next += delay/100000;
            create_triangles_list();
            calculateSpringForces();
            iterateDisplace();
            drawTrianglesAndPoints();
            printf("This is iteration: %d \n", interationNumber);
            free(triangleIndexList);
            glfwSwapBuffers(win);
        }
        glfwPollEvents();
    }
    
    glfwDestroyWindow(win);
    glfwTerminate();
}

