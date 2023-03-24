/*
 * Elementary simulation using GLFW + OpenGL for display modified for use in PlantSim by Finley Webb
 * Original code by Francois J Nedelec, Cambridge University, 13 Nov 2021, 11 Oct 2022
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#define DEBUG false
#define DISPLAY true /// set to true to display
#define BENCHMARK false /// set to true to benchmark (not bottlenecked by printing or displaying)
#define REGULAR_LATTICE true
#define MOVING_POINTS false
#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <vector>

#include "random.h"
#include "vector.h"
#include "param.h"
#include "hormone.h"
#include "object.h"
#include "polish.h"
#include "arrays.h"
#include "Clarkson-Delaunay.cpp"
#include "graphics.h"
#include "springs.h"


///-----------------------------------------------------------------------------

/// evolves system, stepping points forward and accelerating velocity
static void animate(){
    realTime += delta;
    for ( int i = 0; i < nbo; ++i ) {
        pointsArray[i].step();
    }
}

void initRegularTriangularLattice() {
    int index = 0;
    bool isSqrtNBOWhole = !fmod(sqrt(nbo), 1);

    int numPointsX = sqrt(nbo);
    int numPointsY = sqrt(nbo);
    double spacing = pointsArray[0].cellRadius*0.5;
    double xSum = 0.0, ySum = 0.0;
    int numPoints = numPointsX * numPointsY;
    if (isSqrtNBOWhole){
        for (int i = 0; i < numPointsX; i++) {
            for (int j = 0; j < numPointsY; j++) {
                double x = i * spacing + ((j % 2 == 0) ? 0 : spacing / 2.0);
                double y = j * spacing * sin(M_PI / 3.0);
                Point& p = pointsArray[index];
                p.disVec = vector2D(x, y);
                xSum += x;
                ySum += y;
                index++;
            }
        }
        double xCenter = xSum / numPoints;
        double yCenter = ySum / numPoints;
            for (int i = 0; i < numPoints; i++) {
                pointsArray[i].disVec -= vector2D(xCenter, yCenter);
            }
        }
    else{
        printf("NBO DOES NOT EQUAL numPointsX * numPointsY\n");
    }
}

void initPerfectCircle(double circleRadius) {
    int index = 0;
    double angleSpacing = 2 * M_PI / nbo;
    double xSum = 0.0, ySum = 0.0;

    for (int i = 0; i < nbo; i++) {
        double angle = 2 * i * angleSpacing;
        double x = circleRadius * cos(angle);
        double y = circleRadius * sin(angle);
        Point& p = pointsArray[index];
        p.disVec = vector2D(x, y);
        xSum += x;
        ySum += y;
        index++;
    }

    double xCenter = xSum / nbo;
    double yCenter = ySum / nbo;
    for (int i = 0; i < nbo; i++) {
        pointsArray[i].disVec -= vector2D(xCenter, yCenter);
    }
}

void initHollowSquare(double sideLength, int nbo) {
    int index = 0;
    int pointsPerSide = nbo / 4;
    double spacing = sideLength / (pointsPerSide - 1);
    double xSum = 0.0, ySum = 0.0;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < pointsPerSide; j++) {
            double x, y;

            if (i == 0) {
                x = j * spacing - sideLength / 2;
                y = -sideLength / 2;
            } else if (i == 1) {
                x = sideLength / 2;
                y = j * spacing - sideLength / 2;
            } else if (i == 2) {
                x = sideLength / 2 - j * spacing;
                y = sideLength / 2;
            } else { // i == 3
                x = -sideLength / 2;
                y = sideLength / 2 - j * spacing;
            }

            Point& p = pointsArray[index];
            p.disVec = vector2D(x, y);
            xSum += x;
            ySum += y;
            index++;
        }
    }

    double xCenter = xSum / nbo;
    double yCenter = ySum / nbo;
    for (int i = 0; i < nbo; i++) {
        pointsArray[i].disVec -= vector2D(xCenter, yCenter);
    }
}

/// creates an array of xy co-ords for the delaunay triangulation function, then execute it
void create_triangles_list(){
    float xyValuesArray[nbo][2];
    for ( int i = 0; i < nbo; i++){
        xyValuesArray[i][0] = pointsArray[i].disVec.xx;
        xyValuesArray[i][1] = pointsArray[i].disVec.yy;
    }

    numTriangleVertices = 0;
    triangleIndexList = BuildTriangleIndexList((void*)xyValuesArray, (float)1.0, nbo, (int)2, (int)1, &numTriangleVertices);

#if DEBUG
    printf("\nThere are %d points moving around \n", nbo);
    printf("\nThe number of vertices defined by numTriangleVertices is %d\n", numTriangleVertices);
    /*printf("int has size %ld \n", sizeof(int ));
    printf("\ntriangleIndexList contains the values: ");
    for (int i = 0; i < numTriangleVertices; i++)
        printf("%u, ", triangleIndexList[i]);
    printf("\n");*/
#endif ///DEBUG
}

/// create a neighbourhood array (seperated out from CalculateSpringForces function)
void fill2DArrayNeighbourhoods(int** neighbourhoods, int* total, int rows){
    for (int v = 0; v < numTriangleVertices; v+=3) {
        /// add neighbours of the first value of the triangle to its row in the neighbourhood array
        total[triangleIndexList[v]]++;
        neighbourhoods[triangleIndexList[v]][total[triangleIndexList[v]]] = triangleIndexList[v + 1];
        total[triangleIndexList[v]]++;
        neighbourhoods[triangleIndexList[v]][total[triangleIndexList[v]]] = triangleIndexList[v + 2];
        /// add neighbours of the second value in triangleIndex List to its row in the neighbour array
        total[triangleIndexList[v + 1]]++;
        neighbourhoods[triangleIndexList[v + 1]][total[triangleIndexList[v + 1]]] = triangleIndexList[v];
        total[triangleIndexList[v + 1]]++;
        neighbourhoods[triangleIndexList[v + 1]][total[triangleIndexList[v + 1]]] = triangleIndexList[v + 2];
        /// add neighbours of the third value
        total[triangleIndexList[v + 2]]++;
        neighbourhoods[triangleIndexList[v + 2]][total[triangleIndexList[v + 2]]] = triangleIndexList[v];
        total[triangleIndexList[v + 2]]++;
        neighbourhoods[triangleIndexList[v + 2]][total[triangleIndexList[v + 2]]] = triangleIndexList[v + 1];
    }
#if DEBUG
        printf("Neighbourhood array BEFORE cleaning: \n");
    for (int n = 0; n < nbo; n++){
        printf("nbo %d:  ", n);
        for (int i = 0; i < NAW; i++){
            printf(" %d", neighbourhoods[n][i]);
        }
        printf("\n");
    }
    printf("\n\n");
#endif
        /// remove duplicates
        for (int i = 0; i < nbo; i++) {    /// for each row
            for (int j = 0; j < (total[i]+1); j++) {    /// for each value in the row
                for (int k = j+1; k < (total[i]+1); k++) {    /// for each subsequent value until the end of filled elements
                    if (neighbourhoods[i][j] == neighbourhoods[i][k]) {    /// check if there are pairwise duplicates
                        for (int l = k; l < (total[i]+1); l++) {
                            neighbourhoods[i][l] = neighbourhoods[i][l+1];  /// override duplicate and shift all values left
                            neighbourhoods[i][total[i]] = -1;  /// left most value is converted to empty element
                        }
                        total[i]--; /// decrement pointer for the array to account for this
                        j--;    /// pointer to possible duplicates shifts left one
                    }
                }
            }
        }
#if DEBUG
    printf("Neighbourhood array AFTER cleaning: \n");
    for (int n = 0; n < nbo; n++){
        printf("nbo %d:  ", n);
        for (int i = 0; i < NAW; i++){
            printf(" %d", neighbourhoods[n][i]);
        }
        printf("\n");
    }
    printf("\n\n");
#endif
}

void iterateDisplace(){
    for(int i = 0; i<nbo; i++){
        pointsArray[i].step();
    }
}

double trackTime(){
    return currentTime += timestep;
}

void startHormoneBD(double inputStartTime){
    static bool flag = false;
    if ((currentTime > inputStartTime) and (flag == false)){
        flag = true;
        /// find the point closest to the hormone Origin
        int closest_point_index = -1;
        double squareMinDist = 1000*1000*xBound;
        for (int i = 0; i < nbo; i++) {
            double squareDisFromOrigin = (pointsArray[i].disVec - horm2Source1).magnitude_squared();
            if (squareDisFromOrigin < squareMinDist) {
                squareMinDist = squareDisFromOrigin;
                closest_point_index = i;
            }
        }
    /// set this point as the hormone producer
    pointsArray[closest_point_index].isHormone1Producer = true;
        printf("Closest point is point %d\n", closest_point_index);
    }
    else{
    }
}

void calcHormBirthDeath(double inputStartTime){
    for (int i = 0; i < nbo; i++){
        Point& cell = pointsArray[i]; /// alias for pointsArray[i]
        /// calculate amount of hormone made by producers
        if ((cell.isHormone1Producer == true)){
            cell.produceHormone1BD(hormone1ProdRate);
            cell.degradeHormone1BD(hormone1DegRate);
        }
        else{
            cell.degradeHormone1BD(hormone1DegRate);
        }
    }
}

void hormReactDiffuse(double inputStartTime) {
    static bool flag = false;
    if ((currentTime > inputStartTime) and (flag == false)) {
        flag = true;
        /// find the point closest to the hormone Origin
        int closest_point_source1_index = -1;
        int closest_point_source2_index = -1;
        double squareMinDist1 = 1000 * 1000 * xBound;
        for (int i = 0; i < nbo; i++) {
            double squareDisFromOrigin = (pointsArray[i].disVec - horm2Source1).magnitude_squared();
            if (squareDisFromOrigin < squareMinDist1) {
                squareMinDist1 = squareDisFromOrigin;
                closest_point_source1_index = i;
            }
        }
        double squareMinDist2 = 1000 * 1000 * xBound;
        for (int k = 0; k < nbo; k++) {
            double squareDisFromOrigin = (pointsArray[k].disVec - horm2Srouce2).magnitude_squared();
            if (squareDisFromOrigin < squareMinDist1) {
                squareMinDist2 = squareDisFromOrigin;
                closest_point_source2_index = k;
            }
            /// set this point as the hormone producer
            pointsArray[closest_point_source1_index].isHormone2Producer = true;
            pointsArray[closest_point_source2_index].isHormone2Producer = true;
        }
    }
        for (int i = 0; i < nbo; i++) {
            Point &cell = pointsArray[i]; /// alias for pointsArray[i]
            /// in reaction diffusion all cells produce horm1
            if (cell.isHormone2Producer == true) {
                cell.produceHormone1ReactD( RDfeedRate);
                cell.productHormone2ReactD( 2*RDfeedRate);
                cell.react1With2( reactRate1to2);
                cell.degradeHormone2ReactD( RDkillRate,  RDfeedRate);
                //printf("Point %d is a horm2 producer\n", i);
                //printf("Point %d is a horm2 producer\n", i);
            } else {
                cell.produceHormone1ReactD( RDfeedRate);
                cell.react1With2( reactRate1to2);
                cell.degradeHormone2ReactD( RDkillRate,  RDfeedRate);
            }
        }
}

void v1DiffuseHorm(int** neighbourhoods) {

    for (int i = 0; i < nbo; i++) { ///for each primary point in pointsArray (iterates through each point using i)
        Point &centre = pointsArray[i]; /// alias for pointsArray[i]
        for (int l = 0; l < NAW; l++) {
            Point &neighbour = pointsArray[neighbourhoods[i][l]];
            if (neighbourhoods[i][l] != -1) {
                /// using squared magnitudes here is computationally faster
                if ((neighbour.disVec - centre.disVec).magnitude_squared() <
                    (0.2 * centre.cellRadius * 0.2 * centre.cellRadius)) {
                } /// stops diffusion if points overlap
                else {
                    /// find the magnitude of distance between the neighbouring point and the central point
                    double magnitudeOfDistance = (centre.disVec - neighbour.disVec).magnitude(); // m

                    /// find difference in hormone amount between cells
                    double hormone1ConcnDiff = centre.myTotalHormone1 - neighbour.myTotalHormone1;  //n / m
                    double hormone2ConcnDiff = centre.myTotalHormone2 - neighbour.myTotalHormone2;

                    double hormone1ConcnGrad = hormone1ConcnDiff / magnitudeOfDistance; //n / m^2
                    double hormone2ConcnGrad = hormone2ConcnDiff / magnitudeOfDistance;
                    /// diffuse the hormone from the centre to neighbour
                    neighbour.myDeltaHormone1 += timestep*(hormone1DiffCoeff * hormone1ConcnGrad * centre.cellRadius); //  n = t * (m^2/t * n/m * m)
                    centre.myDeltaHormone1 -= timestep*(hormone1DiffCoeff * hormone1ConcnGrad * centre.cellRadius);

                    neighbour.myDeltaHormone2 += timestep*(hormone2DiffCoeff * hormone2ConcnGrad * centre.cellRadius); //  n = t * (m^2/t * n/m * m)
                    centre.myDeltaHormone2 -= timestep*(hormone2DiffCoeff * hormone2ConcnGrad * centre.cellRadius);
                }
            }
        }
    }
    double sumHorm1 = 0;
    double sumHorm2 = 0;

    for (int j = 0; j < nbo; j++) {
        Point &cell = pointsArray[j];

        sumHorm1 += cell.myTotalHormone1;
        sumHorm2 += cell.myTotalHormone2;
    }
printf("The sum of hormone1 is %f\nThe sum of hormone 2 is %f \n", sumHorm1, sumHorm2); /// test conservation of hormone
}

int findMaxHormone(){
    int maxPointer = 0;
    for (int i = 0; i<nbo; i++){
        if (pointsArray[i].myTotalHormone1 > pointsArray[maxPointer].myTotalHormone1){
            maxPointer = i;
        }
    }
    return maxPointer;
}

void globalUpdateHormone(){
    for (int i = 0; i<nbo; i++){
        pointsArray[i].updateTotalHormone();
    }
}

void hormoneExpandEffect(){
    for (int i = 0; i < nbo; i++){
    Point& centre = pointsArray[i];
    centre.cellRadius = centre.cellRadiusBase + (horm1Efficacy * centre.myTotalHormone1 * SCALING_FACTOR);
    }
}

// TODO add a check so that cells cannot divide immediately after dividing again
void calcMitosis(){
    for (int i = 0; i < nbo; i++){
    Point &motherCell = pointsArray[i];
        if (myPrand() < motherCell.divisionProb(baseMaxProbOfDiv, nbo, baseDesiredTotalCells)){

            nbo++; /// MAX points already exist, need to increase pointer by one to access new cell

            Point& daughterCell = pointsArray[nbo-1];
            vector2D normOrient = vector2D(3*mySrand(), mySrand()).normalise();

            vector2D displaceVec = 0.15 * motherCell.cellRadius * normOrient;
            daughterCell.disVec = motherCell.disVec + displaceVec; /// change daughter cell to inherit mother cell position + random orientation
            motherCell.disVec -= displaceVec;  /// mother cell displaced in opposite direction
            // TODO add something that can alter orientation of division
            // TODO maybe add something at causes orientation of division to align with tension
        }
    }
}

bool comparePointsByAngle(const Point& a, const Point& b) {
    vector2D reference(0, 1);
    vector2D vecA = a.disVec;
    vector2D vecB = b.disVec;

    double angleA = angleBetweenVecs(reference, vecA);
    double angleB = angleBetweenVecs(reference, vecB);

    return angleA < angleB;
}

void sortPointsByAngle(Point pointsArray[], size_t size) {
    std::sort(pointsArray, pointsArray + size, comparePointsByAngle);
}

void computerDiscreteFourierCoeffs(int iteration, int finalIterationInput){

    if(iteration == finalIterationInput){
        float xx[nbo], yy[nbo];

        for ( int i = 0; i < nbo; i++){
            xx[i] = pointsArray[i].disVec.xx;
            yy[i] = pointsArray[i].disVec.yy;
        }

        double t = 1.0; /// period length
        double sampleFreq = nbo / t;
        double nyquistLim = sampleFreq / 2; /// coefficients representing sample distances greater than 2 per period result in aliasing

        double fourierCoeffs[nbo][2]; /// array of real-valued Fourier coefficients, a read and imaginary component per Coeff

        /// Compute DFT
        for (int k = 0; k < nyquistLim; k++) {
            double sum_re = 0, sum_im = 0;
            for (int n = 0; n < nbo; n++) {
                double angle = 2 * M_PI * k * n / nbo;  /// M_PI is math.h definition of PI to high precision
                sum_re += 2*(xx[n] * cos(angle) + yy[n] * sin(angle)); /// multiplied by 2 to account for nyquist lim
                sum_im += 2*(yy[n] * cos(angle) - xx[n] * sin(angle));
            }
            fourierCoeffs[k][0] = sum_re / nbo;
            fourierCoeffs[k][1] = sum_im / nbo;
        }

        // Print coefficients
        for (int k = 0; k < nyquistLim; k++) {
            double re = fourierCoeffs[k][0];
            double im = fourierCoeffs[k][1];
            printf("c[%d] = %f + %fi\n", k, re, im);
        }
    }
}

void printFourierCoeffs(double xx[], int xxLength, double yy[], int yyLength) {
    if (xxLength != yyLength){
        printf("ERROR! x and y arrays are different lengths\n");
    }
    else {
        double t = 1.0; /// period length
        double sampleFreq = xxLength / t;
        int nyquistLim = (int)(sampleFreq / 2); /// coefficients representing sample distances greater than 2 per period result in aliasing

        double fourierCoeffs[xxLength][2]; /// array of real-valued Fourier coefficients, a real and imaginary component per Coeff

        /// Compute DFT
        for (int k = 0; k < nyquistLim; k++) {
            double sum_re = 0, sum_im = 0;
            for (int n = 0; n < xxLength; n++) {
                double angle = 2 * M_PI * k * n / xxLength;
                double C = cos(angle), S = sin(angle);

                sum_re += xx[n] * C + yy[n] * S;
                sum_im += yy[n] * C - xx[n] * S;
            }
            /// multiplied by 2 to account for nyquist lim
            fourierCoeffs[k][0] =  sum_re / xxLength;
            fourierCoeffs[k][1] =  sum_im / xxLength;
        }

        /// Print coefficients
        for (int k = 0; k < nyquistLim; k++) {
            double re = fourierCoeffs[k][0];
            double im = fourierCoeffs[k][1];
            printf("c[%d] = %f + %fi\n", k, re, im);
        }
    }
}///deprecated

void printDeltaFourierCoeffs(){
    double polarCoords[nbo][2];
    double sinComponents[nbo];
    double cosComponents[nbo];
    double angFreq[nbo];
    double fundamentalFreq = 2*M_PI;
    double T = 1; /// the "total period" of the function
    double dt = T / nbo; /// the timestep for numeric integrration


    for (int i = 0; i < nbo; i++){
        Point& cell = pointsArray[i];
        polarCoords[i][0] =  cell.disVec.magnitude(); ///the radius value
        polarCoords[i][1] = atan2(cell.disVec.yy, cell.disVec.xx); ///the theta value
    }

    /// calculate the sin and cos components of the fourier coeffieints
    for (int k = 0; k < nbo; k++) {
        sinComponents[k] = 0;
        cosComponents[k] = 0;
        for (int n = 0; n < nbo; n++) {
            angFreq[k] = 2*M_PI*k/fundamentalFreq;
            sinComponents[k] += polarCoords[n][0] * sin(angFreq[k] * polarCoords[n][1]) * dt;
            cosComponents[k] += polarCoords[n][0] * cos(angFreq[k] * polarCoords[n][1]) * dt;
        }
    }

    for (int m = 0; m<nbo; m++)
    {
        printf("Magnitude/Phase of coefficient %d: %f   %f\n", m, sqrt(sinComponents[m]*sinComponents[m] + cosComponents[m]*cosComponents[m]),
                                                                         atan2(cosComponents[m], sinComponents[m]));
    }

    /// display the inverse fourier, as an animation of the inverse
    if (displayInverseFourier) {
        double x, y;
        static double t = 0;

        glPointSize(5);
        glBegin(GL_POINTS);
        for (int i = 0; i < nbo; i++) {
            x = 0;
            y = 0;
            for (int k = 0; k < nbo; k++) {
                x += (sinComponents[k] * cos(angFreq[k] * t) - cosComponents[k] * sin(angFreq[k] * t)) * 2 / T;
                y += (sinComponents[k] * sin(angFreq[k] * t) + cosComponents[k] * cos(angFreq[k] * t)) * 2 / T;
            }
            glVertex2f(x, y);
        }
        glEnd();

        t += 0.01; // Increment the time variable for the animation
    }
}

void speedTest(int iterationNumber, int versionOfAlgoUsed, int nboDesired){
    double now =glfwGetTime();
    for (int i = 0; i < iterationNumber; i++)
        {
            create_triangles_list();
            if (versionOfAlgoUsed == 1){
                v1CalcSprings();
                iterateDisplace();
            }
            else if (versionOfAlgoUsed == 2){
                v2CalcSprings();
                iterateDisplace();
            }
            else if (versionOfAlgoUsed == 3){
                int** neighbourhoods = create2Darray(nbo, NAW);
                init2DArray(neighbourhoods, nbo, NAW, -1);
                int* totalArray = create1Darray(nbo);
                init1DArray(totalArray, nbo, -1);
                fill2DArrayNeighbourhoods(neighbourhoods, totalArray, NAW);
                v3CalcSprings(neighbourhoods);
                iterateDisplace();
                free(neighbourhoods);
                free(totalArray);
            }
        }
    double cpu = glfwGetTime() - now;
    printf("Iterations = %d\n Time taken = %f \n", iterationNumber, cpu);

}



/* program entry */
/// argc is the number of arguements, argv    y = yBound * srand(); is pointer to array of strings
int main(int argc, char *argv[]) {
        for (int i = 1; i < argc; ++i) {
            const char *arg = argv[i];
            size_t n = strlen(arg);
            if (n > 4 && 0 == strcmp(arg + n - 4, ".cym"))
                readFile(arg);
            else if (0 == readOption(arg))
                printf("Argument '%s' was ignored\n", arg);
        }
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return EXIT_FAILURE;
    }
    glfwSetErrorCallback(error);

    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    //glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    //glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);

    GLFWwindow *win = glfwCreateWindow(winW, winH, "LifeSim", NULL, NULL);
    if (!win) {
        fprintf(stderr, "Failed to open GLFW window\n");
        glfwTerminate();
        return EXIT_FAILURE;
    }
    init(win);

#if BENCHMARK
    for (int i = 1; i < 11; i++) {
        nbo = 100 * i;
        printf("Points to be simulated: %d\n", nbo);
        speedTest(1000, false, 10);
        printf("\n");
    }
#endif

#if DISPLAY
    double next = 0;
    while (!glfwWindowShouldClose(win)) {
        static int iterationNumber = 1;
        double now = glfwGetTime();
        if (now > next) {
            while (iterationNumber <= 100 * finalIterationNumber) {
#if REGULAR_LATTICE
                if (iterationNumber == 1) {
                    //initPerfectCircle(20*SCALING_FACTOR);
                    initHollowSquare(20 * SCALING_FACTOR, nbo);
                }
#endif
                iterationNumber++;
                next += delay / 100000;
                trackTime();

                printf("nbo is %d\n", nbo);
                create_triangles_list();
                int **neighbourhoods = create2Darray(nbo, NAW);
                init2DArray(neighbourhoods, nbo, NAW, -1);
                int *totalArray = create1Darray(nbo);
                init1DArray(totalArray, nbo, -1);
                fill2DArrayNeighbourhoods(neighbourhoods, totalArray, NAW);

#if MOVING_POINTS
                v3CalcSprings(neighbourhoods);
#endif

                iterateDisplace();
                v1DiffuseHorm(neighbourhoods);
                hormReactDiffuse(hormone1IntroTime);
                //calcMitosis();
                globalUpdateHormone();

                drawPoints();

                free(triangleIndexList);
                free(neighbourhoods);
                free(totalArray);
                glfwSwapBuffers(win);
                if (iterationNumber >= finalIterationNumber) {
                    printDeltaFourierCoeffs();
                }
                if (displayInverseFourier) {
                    glfwSwapBuffers(win);
                    glfwPollEvents();
                    printf("Displaying iteration %d\n", iterationNumber);
                }
            }
        }
    }
    if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(win, GLFW_TRUE);
    }
    glfwDestroyWindow(win);
    glfwTerminate();
#endif
}