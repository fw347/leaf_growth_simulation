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
#define MOVING_POINTS true
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
#include "springs.h"
#include "graphics.h"


///-----------------------------------------------------------------------------

/// evolves system, stepping points forward and accelerating velocity
static void animate(){
    realTime += delta;
    for ( int i = 0; i < nbo; ++i ) {
        pointsArray[i].step();
    }
}
// TODO intiate random cloud of points in circle
// TODO maybe use a reaction diffusion method to find edge of the shape

void initRegularTriangularLattice() {
    int index = 0;
    bool isSqrtNBOWhole = !fmod(sqrt(nbo), 1);

    int numPointsX = sqrt(nbo);
    int numPointsY = sqrt(nbo);
    double spacing = pointsArray[0].cellRadius*2;
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
    int pointsPerSide = nbo / 4;
    double spacing = sideLength / (pointsPerSide);
    double xSum = 0.0, ySum = 0.0;

    for (int i = 0; i < nbo; i++) {
        int side = i / pointsPerSide;
        int j = i % pointsPerSide;
        double x, y;

        if (side == 0) {
            x = j * spacing - sideLength / 2;
            y = -sideLength / 2;
        } else if (side == 1) {
            x = sideLength / 2;
            y = j * spacing - sideLength / 2;
        } else if (side == 2) {
            x = sideLength / 2 - j * spacing;
            y = sideLength / 2;
        } else { // side == 3
            x = -sideLength / 2;
            y = sideLength / 2 - j * spacing;
        }

        Point& p = pointsArray[i];
        p.disVec = vector2D(x, y);
        xSum += x;
        ySum += y;
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
//printf("The sum of hormone1 is %f\nThe sum of hormone 2 is %f \n", sumHorm1, sumHorm2); /// test conservation of hormone
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

// TODO 3) shape is not centered on 0,0
// TODO 4) arguments not being read correct by readFile function?
// TODO 5)

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


double** computeDeltaFourierCoeffs(int desiredNumFourierCoeffs) { /// "nonuniform discrete Fourier transform of type II (NUDFT-II)"
    /// mallocing the memory for the pointers to each row
    double **FourierCoeffs = (double **) malloc(desiredNumFourierCoeffs * sizeof(double *));
    for (int i = 0; i < desiredNumFourierCoeffs; i++) {
        FourierCoeffs[i] = (double *) malloc(2 * sizeof(double));
    }
    double polarCoords[nbo][2];

    sortPointsByAngle(pointsArray, nbo);

    for (int i = 0; i < nbo; i++) {
        Point &cell = pointsArray[i];
        polarCoords[i][0] = cell.disVec.magnitude(); ///the radius value
        polarCoords[i][1] = atan2(cell.disVec.yy, cell.disVec.xx); ///the theta value
    }

    double maxRadiusValue = 0; /// find max radius value to scale radius_n values to between 0 and 1
    for (int jj = 0; jj<nbo; jj++){
        if (polarCoords[jj][0] > maxRadiusValue){
            maxRadiusValue = polarCoords[jj][0];
        }
    }

    /// calculate the sin and cos components of the fourier coefficients
    for (int k = 0; k < desiredNumFourierCoeffs; k++) {
        double &realComp = FourierCoeffs[k][0];
        double &imgComp = FourierCoeffs[k][1];
        realComp = 0;
        imgComp = 0;

        if (k == 0) {
            for (int n = 0; n < nbo; n++) {
                double &radiusN = polarCoords[n][0];
                double &thetaN = polarCoords[n][1];

                realComp += 1.0/nbo * radiusN;

            }
        } else {
            for (int n = 0; n < nbo; n++) {
                double &radiusN = polarCoords[n][0];
                double &thetaN = polarCoords[n][1];
                realComp += 1.0/nbo * radiusN * cos(k * thetaN);
                imgComp += 1.0/nbo * radiusN * sin(k * thetaN);
            }
        }
    }
    return FourierCoeffs;
}

void printDeltaFourierCoeffs(double** inputFourierArray, int desiredNumOfFourierCoeffs){
    for (int m = 0; m<desiredNumOfFourierCoeffs; m++) {
        double &realValue = inputFourierArray[m][0];
        double &imgValue = inputFourierArray[m][1];
        {
            printf("Magnitude/Phase of coefficient %d: %f   %f\n",
                   m, sqrt(realValue*realValue + imgValue*imgValue), atan2(imgValue, realValue));
        }
    }
}

void reconstructShape(double** inputFourierArray, int desiredNumOfFourierCoeffs){
    double x, y;
    int numPoints = 3000; /// higher = smoother curve
    double T = 1;
    double dTheta = 2*M_PI / numPoints; /// stepsize for the curve, should be sized to loop once
    double currentTheta = 0;

    double &a_0_real = inputFourierArray[0][0];
    double a_0 = sqrt(a_0_real*a_0_real);

    double reconstructedPolarCoords[numPoints][2];


    /// f(t) = a_0 + Σ(a_n*cos(2*pi*n*t/T) + b_n*sin(2*pi*n*t/T))
    for (int n = 0; n < numPoints; n++) {
        double reconstructedRadiusN = a_0; /// doubling first coeff to account for how this is the average

        for (int k = 1; k < desiredNumOfFourierCoeffs; k++) {
            double &realComp = inputFourierArray[k][0];
            double &imgComp = inputFourierArray[k][1];

            reconstructedRadiusN += realComp * cos(k * currentTheta) + imgComp * sin(k * currentTheta);
        }
        reconstructedPolarCoords[n][0] = reconstructedRadiusN;
        reconstructedPolarCoords[n][1] = currentTheta;
        currentTheta += dTheta;
    }

    drawSquare(xBound, yBound);
    glLineWidth(3);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < numPoints; i++) {
        double theta = i * dTheta; /// Calculate the angle t for the current step
        x = reconstructedPolarCoords[i][0] * cos(theta);
        y = reconstructedPolarCoords[i][0] * sin(theta);
        glColor3f(1.0,1.0,1.0);
        glVertex2f(x, y);
    }
    glEnd();
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
// TODO go back to initial ifrap glfw code, make sure window creation is termination
#if DISPLAY
    double next = 0;
    while (!glfwWindowShouldClose(win)) {
        static int iterationNumber = 1;
        double now = glfwGetTime();
        if (now > next) {
            while (iterationNumber <= 50 * finalIterationNumber) {
#if REGULAR_LATTICE
                if (iterationNumber == 1) {
                    //initPerfectCircle(20*SCALING_FACTOR);
                    //initHollowSquare(20 * SCALING_FACTOR, nbo);
                    //initRegularTriangularLattice();
                }
#endif
                glClear(GL_COLOR_BUFFER_BIT);
                iterationNumber++;
                next += delay / 100000;
                trackTime();

                //printf("nbo is %d\n", nbo);
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
                calcMitosis();
                globalUpdateHormone();

                drawPoints(); // calls

                free(triangleIndexList);
                free(neighbourhoods);
                free(totalArray);

                if (iterationNumber >= 0/*finalIterationNumber*/) {
                    int fourierCoeffsNum = 0.5*nbo;
                    if (nbo > 2*maxFourierCoeffs){
                        fourierCoeffsNum = maxFourierCoeffs;
                    }
                    printf("\nInverse should be displaying\n");
                    double **fourierCoeffs = computeDeltaFourierCoeffs(fourierCoeffsNum);
                    printDeltaFourierCoeffs(fourierCoeffs, fourierCoeffsNum);
                    printf("\n\n");
                    if (displayInverseFourier) {
                        glfwPollEvents();
                        reconstructShape(fourierCoeffs, fourierCoeffsNum);
                    }
                    free(fourierCoeffs);
                }
                glFlush();
                glfwSwapBuffers(win);

            }
        }
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(win, GLFW_TRUE);
        }

#endif
    }
    glfwDestroyWindow(win);
    glfwTerminate();
}