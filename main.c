#include <GL/glut.h>  // might need GL/glut.h
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#define GRAVITY -0.005
#define SPEED_FACTOR 0.1
#define FRICTION_FACTOR 0.9
#define GROUND_SIZE 15.0
#define SPHERE_RADIUS 2.0
#define X 0
#define Y 1
#define Z 2

struct glob {
    float angle[3];
    int axis;
};

struct glob global = { {0.0,0.0,0.0},Y };

struct Particle {
    float px, py, pz;                   // Position
    float dx, dy, dz;                   // Direction
    float speed;                        // Speed
    float angleX, angleY, angleZ;       // Rotation angles
    float dAngleX, dAngleY, dAngleZ;    // Angle increments
    float color[3];                     // RGB Color
    bool active;                        // Active state
};

struct ParticleList {
    struct ParticleNode* head;
    struct ParticleNode* tail;
    int size;
};

struct ParticleNode {
    struct ParticleNode* next;
    struct ParticleNode* prev;
    struct Particle particle;
};

struct ParticleList particleList;
struct ParticleNode* selectedParticleNode = NULL;

bool constantStream = true;
bool manualFiring = false;
bool randomSpeedMode = false;
bool randomSpinMode = true;
bool frictionMode = true;
bool backfaceCulling = false;
bool particleView = false;
bool sprayMode = false;

GLfloat savedModelviewMatrix[16];
int currentRenderMode = 3;
int currentShadingMode = 1;

void toggleShadingMode() {
    if (currentShadingMode == 0) {
        glShadeModel(GL_SMOOTH);
        currentShadingMode = 1;
    }
    else {
        glShadeModel(GL_FLAT);
        currentShadingMode = 0;
    }
}

// Enable or disable backface culling
void toggleBackfaceCulling() {
    backfaceCulling = !backfaceCulling;
    if (backfaceCulling) {
        glEnable(GL_CULL_FACE);
        glFrontFace(GL_CCW);
        glCullFace(GL_BACK);
    }
    else {
        glDisable(GL_CULL_FACE);
    }
}

// Reset particle list and modes
void resetSimulation() {
    struct ParticleNode* currentNode = particleList.head;
    while (currentNode != NULL) {
        struct ParticleNode* nextNode = currentNode->next;
        free(currentNode);
        currentNode = nextNode;
    }

    particleList.head = particleList.tail = NULL;
    particleList.size = 0;

    constantStream = true;
    manualFiring = false;
    randomSpinMode = true;
    frictionMode = true;
    sprayMode = false;
}

// Create a new particle and add it to the list
void createParticle() {
    struct ParticleNode* newNode = (struct ParticleNode*)malloc(sizeof(struct ParticleNode));
    if (newNode == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for a new particle node.\n");
        exit(EXIT_FAILURE);
    }

    float speed;
    float dx;
    float dz;

    if (!sprayMode) {
        dx = ((rand() % 100) / 100.0) - 0.5;
        dz = ((rand() % 100) / 100.0) - 0.5;
    }
    else {
        dx = ((rand() % 200) / 100.0) - 1.0;
        dz = ((rand() % 200) / 100.0) - 1.0;
    }

    if (randomSpeedMode) {
        speed = 1.0 + ((rand() % 10) / 10.0);
    }
    else {
        speed = 1.0;
    }

    newNode->particle = (struct Particle){
        .px = 0.0,
        .py = 0.5,
        .pz = 0.0,
        .dx = dx,
        .dy = 1.0,
        .dz = dz,
        .speed = speed,
        .angleX = 0.0,
        .angleY = 0.0,
        .angleZ = 0.0,
        .dAngleX = 2.0,
        .dAngleY = 3.0,
        .dAngleZ = 1.5,
        .color = {((rand() % 100) / 100.0), ((rand() % 100) / 100.0), ((rand() % 100) / 100.0)},
        .active = true,
    };

    newNode->next = NULL;
    newNode->prev = particleList.tail;

    if (particleList.size == 0) {
        particleList.head = newNode;
    }
    else {
        particleList.tail->next = newNode;
    }

    particleList.tail = newNode;
    particleList.size++;
}

// Check if particle is within hole extents
bool isParticleWithinHoleExtents(const struct Particle* particle) {
    return (particle->px >= 5.0 && particle->px <= 10 &&
        particle->pz >= 5.0 && particle->pz <= 10);
}

// Check if particle is within ground extents
bool isParticleWithinGroundExtents(const struct Particle* particle) {
    return (particle->px >= -GROUND_SIZE && particle->px <= GROUND_SIZE &&
        particle->pz >= -GROUND_SIZE && particle->pz <= GROUND_SIZE);
}

// Remove inactive particles from the linked list
void removeInactiveParticles() {
    struct ParticleNode* currentNode = particleList.head;
    struct ParticleNode* nextNode;

    while (currentNode != NULL) {
        nextNode = currentNode->next;

        if (!currentNode->particle.active) {
            // Node is inactive, remove it from the linked list
            if (currentNode->prev != NULL) {
                currentNode->prev->next = currentNode->next;
            }
            else {
                // The node is the head of the list
                particleList.head = currentNode->next;
            }

            if (currentNode->next != NULL) {
                currentNode->next->prev = currentNode->prev;
            }
            else {
                // The node is the tail of the list
                particleList.tail = currentNode->prev;
            }

            free(currentNode);
            particleList.size--;
        }

        currentNode = nextNode;
    }
}

float squaredDistance(float x1, float y1, float z1, float x2, float y2, float z2) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    float dz = z1 - z2;
    return dx * dx + dy * dy + dz * dz;
}

void applyFriction(struct ParticleNode* node) {
    node->particle.speed *= FRICTION_FACTOR;
    node->particle.dx *= FRICTION_FACTOR;
    node->particle.dy *= FRICTION_FACTOR;
    node->particle.dz *= FRICTION_FACTOR;
}

void handleGroundCollision(struct ParticleNode* node) {
    if (node->particle.py < 0.1 && node->particle.py > -0.1 &&
        isParticleWithinGroundExtents(&node->particle) &&
        !isParticleWithinHoleExtents(&node->particle)) {
        node->particle.py = 0.1;
        node->particle.dy = -node->particle.dy;  // Bounce back;
        if (frictionMode == true) {
            applyFriction(node);    // Apply friction
        }
    }
}

void handleSphereCollision(struct ParticleNode* node) {
    float minDistanceSquared = (SPHERE_RADIUS * SPHERE_RADIUS) + 0.1;  // Squared radius of the spheres

    // Center of first sphere
    float sphere1CenterX = -10.0;
    float sphere1CenterY = 2.0;
    float sphere1CenterZ = -10.0;

    // Center of second sphere
    float sphere2CenterX = 5.0;
    float sphere2CenterY = 2.0;
    float sphere2CenterZ = -5.0;

    // Squared distance to first sphere
    float distance1Squared = squaredDistance(
        node->particle.px, node->particle.py, node->particle.pz,
        sphere1CenterX, sphere1CenterY, sphere1CenterZ);

    // Squared distance to second sphere
    float distance2Squared = squaredDistance(
        node->particle.px, node->particle.py, node->particle.pz,
        sphere2CenterX, sphere2CenterY, sphere2CenterZ);

    if (distance1Squared < minDistanceSquared || distance2Squared < minDistanceSquared) {
        // Particle is inside a sphere, handle collision

        // Bounce back (opposite direction)
        node->particle.dx = -node->particle.dx;
        node->particle.dy = -node->particle.dy;
        node->particle.dz = -node->particle.dz;

        // Apply friction
        if (frictionMode == true) {
            applyFriction(node);
        }

        // Move the particle slightly away to prevent sticking
        float offset = 0.05;
        node->particle.px += offset * node->particle.dx;
        node->particle.py += offset * node->particle.dy;
        node->particle.pz += offset * node->particle.dz;
    }
}

// Update particle position and state
void updateParticle(struct ParticleNode* node) {
    if (node->particle.active) {
        // Apply gravity
        node->particle.dy += GRAVITY;

        // Update position based on direction and speed
        node->particle.px += node->particle.dx * node->particle.speed * SPEED_FACTOR;
        node->particle.py += node->particle.dy * node->particle.speed * SPEED_FACTOR;
        node->particle.pz += node->particle.dz * node->particle.speed * SPEED_FACTOR;

        // Check for collision with ground
        handleGroundCollision(node);             

        // Check for collision with the spheres
        handleSphereCollision(node);

        // Delete particle if it becomes stationary
        if (node->particle.speed < 0.1) {
            node->particle.active = false;
        }

        // Check for death conditions
        if (node->particle.py < -75.0) {
            node->particle.active = false;
        }              

        // Apply random spin mode
        if (randomSpinMode) {
            node->particle.angleX += node->particle.dAngleX;
            node->particle.angleY += node->particle.dAngleY;
            node->particle.angleZ += node->particle.dAngleZ;

            // Ensure angles are between 0 to 360 degrees
            node->particle.angleX = fmod(node->particle.angleX, 360.0);
            node->particle.angleY = fmod(node->particle.angleY, 360.0);
            node->particle.angleZ = fmod(node->particle.angleZ, 360.0);
        }
    } 
}

// Render the particle
void renderParticle(struct ParticleNode* node) {
    if (node->particle.active) {
        glPushMatrix();
        glTranslatef(node->particle.px, node->particle.py, node->particle.pz);
        glRotatef(node->particle.angleX, 1.0, 0.0, 0.0);
        glRotatef(node->particle.angleY, 0.0, 1.0, 0.0);
        glRotatef(node->particle.angleZ, 0.0, 0.0, 1.0);

        GLfloat mat_ambient_diffuse[] = { node->particle.color[0], 
            node->particle.color[1], node->particle.color[2], 1.0 };
        GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
        GLfloat mat_shininess[] = { 60.0 };

        glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, mat_ambient_diffuse);
        glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
        glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

        glColor3fv(node->particle.color);

        switch (currentRenderMode) {
        // points mode
        case 1:
            glBegin(GL_POINTS);
            glVertex3f(0.0, 0.0, 0.0);
            glEnd();
            break;
        // wireframe mode
        case 2:
            glBegin(GL_LINES);
            // Front face
            glVertex3f(-0.1, -0.1, 0.1);
            glVertex3f(0.1, -0.1, 0.1);
            glVertex3f(0.1, -0.1, 0.1);
            glVertex3f(0.1, 0.1, 0.1);
            glVertex3f(0.1, 0.1, 0.1);
            glVertex3f(-0.1, 0.1, 0.1);
            glVertex3f(-0.1, 0.1, 0.1);
            glVertex3f(-0.1, -0.1, 0.1);
            // Back face
            glVertex3f(-0.1, -0.1, -0.1);
            glVertex3f(0.1, -0.1, -0.1);
            glVertex3f(0.1, -0.1, -0.1);
            glVertex3f(0.1, 0.1, -0.1);
            glVertex3f(0.1, 0.1, -0.1);
            glVertex3f(-0.1, 0.1, -0.1);
            glVertex3f(-0.1, 0.1, -0.1);
            glVertex3f(-0.1, -0.1, -0.1);
            // Connecting lines
            glVertex3f(-0.1, -0.1, 0.1);
            glVertex3f(-0.1, -0.1, -0.1);
            glVertex3f(0.1, -0.1, 0.1);
            glVertex3f(0.1, -0.1, -0.1);
            glVertex3f(0.1, 0.1, 0.1);
            glVertex3f(0.1, 0.1, -0.1);
            glVertex3f(-0.1, 0.1, 0.1);
            glVertex3f(-0.1, 0.1, -0.1);
            glEnd();
            break;
        // solid mode
        case 3:
            // Front face
            glBegin(GL_POLYGON);
            //glNormal3f(0.0, 0.0, 1.0); // Normal for the front face
            glNormal3f(-0.5774, -0.5774, 0.5774);
            glVertex3f(-0.1, -0.1, 0.1);  // Vertex 1 (Bottom left)
            glNormal3f(0.5774, -0.5774, 0.5774);
            glVertex3f(0.1, -0.1, 0.1);   // Vertex 2 (Bottom right)
            glNormal3f(0.5774, 0.5774, 0.5774);
            glVertex3f(0.1, 0.1, 0.1);    // Vertex 3 (Top right)
            glNormal3f(-0.5774, 0.5774, 0.5774);
            glVertex3f(-0.1, 0.1, 0.1);   // Vertex 4 (Top left)
            glEnd();

            // Back face
            glBegin(GL_POLYGON);
            //glNormal3f(0.0, 0.0, -1.0); // Normal for the back face
            glNormal3f(0.5774, 0.5774, -0.5774);
            glVertex3f(0.1, 0.1, -0.1);   // Vertex 5 (Top right)
            glNormal3f(0.5774, -0.5774, -0.5774);
            glVertex3f(0.1, -0.1, -0.1);  // Vertex 6 (Bottom right)
            glNormal3f(-0.5774, -0.5774, -0.5774);
            glVertex3f(-0.1, -0.1, -0.1); // Vertex 7 (Bottom left)
            glNormal3f(-0.5774, 0.5774, -0.5774);
            glVertex3f(-0.1, 0.1, -0.1);  // Vertex 8 (Top left)
            glEnd();

            // Right face
            glBegin(GL_POLYGON);
            //glNormal3f(1.0, 0.0, 0.0); // Normal for the right face
            glNormal3f(0.5774, -0.5774, 0.5774);
            glVertex3f(0.1, -0.1, 0.1);   // Vertex 2 (Bottom front)
            glNormal3f(0.5774, -0.5774, -0.5774);
            glVertex3f(0.1, -0.1, -0.1);  // Vertex 6 (Bottom back)
            glNormal3f(0.5774, 0.5774, -0.5774);
            glVertex3f(0.1, 0.1, -0.1);   // Vertex 5 (Top back)
            glNormal3f(0.5774, 0.5774, 0.5774);
            glVertex3f(0.1, 0.1, 0.1);    // Vertex 3 (Top front)
            glEnd();

            // Left face
            glBegin(GL_POLYGON);
            //glNormal3f(-1.0, 0.0, 0.0); // Normal for the left face
            glNormal3f(-0.5774, 0.5774, 0.5774);
            glVertex3f(-0.1, 0.1, 0.1);   // Vertex 4 (Top front)
            glNormal3f(-0.5774, 0.5774, -0.5774);
            glVertex3f(-0.1, 0.1, -0.1);  // Vertex 8 (Top back)
            glNormal3f(-0.5774, -0.5774, -0.5774);
            glVertex3f(-0.1, -0.1, -0.1); // Vertex 7 (Bottom back)
            glNormal3f(-0.5774, -0.5774, 0.5774);
            glVertex3f(-0.1, -0.1, 0.1);  // Vertex 1 (Bottom front)
            glEnd();

            // Top face
            glBegin(GL_POLYGON);
            //glNormal3f(0.0, 1.0, 0.0); // Normal for the top face
            glNormal3f(-0.5774, 0.5774, 0.5774);
            glVertex3f(-0.1, 0.1, 0.1);   // Vertex 4 (Front left)
            glNormal3f(0.5774, 0.5774, 0.5774);
            glVertex3f(0.1, 0.1, 0.1);    // Vertex 3 (Front right)
            glNormal3f(0.5774, 0.5774, -0.5774);
            glVertex3f(0.1, 0.1, -0.1);   // Vertex 5 (Back right)
            glNormal3f(-0.5774, 0.5774, -0.5774);
            glVertex3f(-0.1, 0.1, -0.1);  // Vertex 8 (Back left)
            glEnd();

            // Bottom face
            glBegin(GL_POLYGON);
            //glNormal3f(0.0, -1.0, 0.0); // Normal for the bottom face
            glNormal3f(-0.5774, -0.5774, -0.5774);
            glVertex3f(-0.1, -0.1, -0.1); // Vertex 7 (Back left)
            glNormal3f(0.5774, -0.5774, -0.5774);
            glVertex3f(0.1, -0.1, -0.1);  // Vertex 6 (Back right)
            glNormal3f(0.5774, -0.5774, 0.5774);
            glVertex3f(0.1, -0.1, 0.1);   // Vertex 2 (Front right)
            glNormal3f(-0.5774, -0.5774, 0.5774);
            glVertex3f(-0.1, -0.1, 0.1);  // Vertex 1 (Front left)
            glEnd();
            break;
        }

        glPopMatrix();
    }
}

// Update the entire frame
void updateFrame() {
    struct ParticleNode* currentNode = particleList.head;

    while (currentNode != NULL) {
        updateParticle(currentNode);
        currentNode = currentNode->next;
    }

    removeInactiveParticles();

    glutPostRedisplay();
}

// Render ground and hole
void renderGround() {
    GLfloat mat_ambient[] = { 0.0, 0.0, 0.0, 0.0 };
    GLfloat mat_diffuse[] = { 1.0, 1.0, 1.0, 1.0 }; 
    GLfloat mat_specular[] = { 0.0, 0.0, 0.0, 1.0 };

    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, 0.0);

    glNormal3f(0.0, 1.0, 0.0);

    glBegin(GL_POLYGON);
    glColor3f(0.5, 0.5, 0.5);
    glVertex3f(-GROUND_SIZE, 0.0, -GROUND_SIZE);
    glVertex3f(-GROUND_SIZE, 0.0, 0.0);
    glVertex3f(GROUND_SIZE, 0.0, 0.0);
    glVertex3f(GROUND_SIZE, 0.0, -GROUND_SIZE);
    glEnd();

    glBegin(GL_POLYGON);
    glColor3f(0.5, 0.5, 0.5);
    glVertex3f(-GROUND_SIZE, 0.0, 0.0);
    glVertex3f(-GROUND_SIZE, 0.0, GROUND_SIZE);
    glVertex3f(0.0, 0.0, GROUND_SIZE);
    glVertex3f(0.0, 0.0, 0.0);
    glEnd();

    glBegin(GL_POLYGON);
    glColor3f(0.5, 0.5, 0.5);
    glVertex3f(0.0, 0.0, 0.0);
    glVertex3f(0.0, 0.0, 5.0);
    glVertex3f(GROUND_SIZE, 0.0, 5.0);
    glVertex3f(GROUND_SIZE, 0.0, 0.0);
    glEnd();

    glBegin(GL_POLYGON);
    glColor3f(0.5, 0.5, 0.5);
    glVertex3f(0.0, 0.0, 5.0);
    glVertex3f(0.0, 0.0, 10.0);
    glVertex3f(5.0, 0.0, 10.0);
    glVertex3f(5.0, 0.0, 5.0);
    glEnd();

    glBegin(GL_POLYGON);
    glColor3f(0.5, 0.5, 0.5);
    glVertex3f(10.0, 0.0, 5.0);
    glVertex3f(10.0, 0.0, 10.0);
    glVertex3f(GROUND_SIZE, 0.0, 10.0);
    glVertex3f(GROUND_SIZE, 0.0, 5.0);
    glEnd();

    glBegin(GL_POLYGON);
    glColor3f(0.5, 0.5, 0.5);
    glVertex3f(0.0, 0.0, 10.0);
    glVertex3f(0.0, 0.0, GROUND_SIZE);
    glVertex3f(GROUND_SIZE, 0.0, GROUND_SIZE);
    glVertex3f(GROUND_SIZE, 0.0, 10.0);
    glEnd();
}

void renderSphere() {
    GLfloat mat_ambient[] = { 0.3, 0.3, 0.3, 1.0 };    
    GLfloat mat_diffuse[] = { 0.8, 0.8, 0.8, 1.0 };   
    GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };  
    GLfloat mat_shininess = 50.0;

    glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialf(GL_FRONT, GL_SHININESS, mat_shininess);

    glPushMatrix();
    glColor3f(0.0, 0.8, 0.0);
    glTranslatef(-10.0, 2.0, -10.0);
    glutSolidSphere(SPHERE_RADIUS, 20, 20);
    glPopMatrix();

    glPushMatrix();
    glColor3f(0.8, 0.0, 0.0);
    glTranslatef(5.0, 2.0, -5.0);
    glutSolidSphere(SPHERE_RADIUS, 20, 20);
    glPopMatrix();
}

void renderFountain() {
    glColor3f(0.0, 0.0, 1.0);
    glutSolidCube(1.0);
}

// adapted code from:
// https://stackoverflow.com/questions/20082576/how-to-overlay-text-in-opengl
void renderCount() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    glOrtho(0, w, 0, h, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);

    glDisable(GL_LIGHTING);
    glColor3f(1, 1, 1);

    glRasterPos2i(20, 20);
    void* font = GLUT_BITMAP_TIMES_ROMAN_24; // or GLUT_BITMAP_HELVETICA_18;
    char particleCount[50];
    snprintf(particleCount, sizeof(particleCount), "Particle Count: %d", particleList.size);
    for (char* c = particleCount; *c != '\0'; c++)
    {
        glutBitmapCharacter(font, *c);
    }

    glEnable(GL_LIGHTING);

    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Apply rotation
    glMatrixMode(GL_MODELVIEW);
    glRotatef(global.angle[X], 1.0, 0.0, 0.0);
    glRotatef(global.angle[Y], 0.0, 1.0, 0.0);
    glRotatef(global.angle[Z], 0.0, 0.0, 1.0);

    renderGround();
    renderFountain();
    renderSphere();

    // Render particles
    struct ParticleNode* currentNode = particleList.head;
    while (currentNode != NULL) {
        renderParticle(currentNode);
        currentNode = currentNode->next;
    }

    // Particle view
    if (particleView) {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(
            selectedParticleNode->particle.px + 1.0,
            selectedParticleNode->particle.py + 1.0,
            selectedParticleNode->particle.pz + 1.0,
            0, 0, 0, 0.0, 1.0, 0.0);
    }
    renderCount();

    glutSwapBuffers();
}

void toggleParticleView() {
    if (!particleView) {
        selectedParticleNode = particleList.tail;
        particleView = true;
        // Save current modelview matrix
        glGetFloatv(GL_MODELVIEW_MATRIX, savedModelviewMatrix);
    }
    else {
        selectedParticleNode = NULL;
        particleView = false;
        // Restore saved modelview matrix
        glMatrixMode(GL_MODELVIEW);
        glLoadMatrixf(savedModelviewMatrix);
    }
}

// Print keyboard commands to console for user
void printKeyboardOptions() {
    printf("Keyboard Options:\n\n");
    printf("f: Fire particle(s) (hold for continuous, manual mode only)\n");
    printf("c: Toggle constant stream mode: %s\n", constantStream ? "Enabled" : "Disabled");
    printf("m: Toggle manual firing/single shot mode: %s\n", manualFiring ? "Enabled" : "Disabled");
    printf("s: Toggle random speed mode: %s\n", randomSpeedMode ? "Enabled" : "Disabled");
    printf("w: Toggle spray mode: %s\n", !sprayMode ? "Low" : "High");
    printf("p: Toggle random particle spin mode: %s\n", randomSpinMode ? "Enabled" : "Disabled");
    printf("b: Toggle backface culling: %s\n", backfaceCulling ? "Enabled" : "Disabled");
    printf("g: Toggle friction mode %s\n", frictionMode ? "Enabled" : "Disabled");
    printf("l: Toggle shading mode: %s\n", currentShadingMode == 0 ? "Flat" :"Gouraud");
    printf("t: Reset the simulation\n\n");
    printf("v: Toggle particle view: %s\n", particleView ? "Enabled" : "Disabled");
    printf("x, y, z: rotate about x, y, or z axis\n");
    printf("Left mouse: rotate clockwise faster\n");
    printf("Right mouse: rotate counter-clockwise faster\n");
    printf("r: reset perspective\n\n");
    printf("1, 2, 3: Render particles as points, wireframe, or solid: %s\n\n", 
        currentRenderMode == 1 ? "Points" : currentRenderMode == 2 ? "Wireframe" : "Solid");
    printf("q: Exit the program\n");
}

// keyboard commands
void keyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 'w':
        sprayMode = !sprayMode;
        system("cls");
        printKeyboardOptions();
        break;
    case 'l':
        toggleShadingMode();
        system("cls");
        printKeyboardOptions();
        break;
    case 'v':       
        toggleParticleView();
        system("cls");
        printKeyboardOptions();
        break;
    case 'b':
        toggleBackfaceCulling();
        system("cls");
        printKeyboardOptions();
        break;
    case 'x':
        global.axis = X;
        break;
    case 'y':
        global.axis = Y;
        break;
    case 'z':
        global.axis = Z;
        break;
    case 's':
        randomSpeedMode = !randomSpeedMode;
        system("cls");
        printKeyboardOptions();
        break;
    case 'f':
        if (manualFiring) {
            createParticle();
        }
        break;
    case 'c':
        constantStream = !constantStream;
        system("cls");
        printKeyboardOptions();
        break;
    case 'm':
        manualFiring = !manualFiring;
        system("cls");
        printKeyboardOptions();
        break;
    case 'p':
        randomSpinMode = !randomSpinMode;
        system("cls");
        printKeyboardOptions();
        break;
    case 't':
        resetSimulation();
        break;
    case 'g':
        frictionMode = !frictionMode;
        system("cls");
        printKeyboardOptions();
        break;
    case 'r':
        global.angle[X] = 0.0;
        global.angle[Y] = 0.0;
        global.angle[Z] = 0.0;
        glPopMatrix();
        glPushMatrix();
        glLoadIdentity();
        gluLookAt(0.0, 35.0, 25.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);
        break;
    case '1':
        currentRenderMode = 1;
        system("cls");
        printKeyboardOptions();
        break;
    case '2':
        currentRenderMode = 2;
        system("cls");
        printKeyboardOptions();
        break;
    case '3':
        currentRenderMode = 3;
        system("cls");
        printKeyboardOptions();
        break;
    case 'q':
        exit(0);
        break;
    }
}

//timer function for animation
void timerFunc(int value) {
    if (constantStream && !manualFiring) {
        createParticle();
    }

    updateFrame();
    glutTimerFunc(16, timerFunc, 0);  // 60 fps
}

//mouse function from example code rotate2.c
void mouse(int btn, int state, int x, int y) {
    if (state == GLUT_DOWN) {
        if (btn == GLUT_LEFT_BUTTON) {
            global.angle[global.axis] = global.angle[global.axis] + 0.2;
        }
        else if (btn == GLUT_RIGHT_BUTTON) {
            global.angle[global.axis] = global.angle[global.axis] - 0.2;
        }
    }
}

// Initialise light settings
void lightInit() {
    GLfloat position[] = { 0.0, 1.0, 0.0, 0.0 };
    GLfloat ambient[] = { 0.1, 0.1, 0.1, 1.0 };
    GLfloat diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat specular[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat lmodel_ambient[] = { 0.2, 0.2, 0.2, 1.0 };

    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);

    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);    
}

int main(int argc, char** argv) {
    printKeyboardOptions();
    glutInit(&argc, argv);
    glutInitWindowSize(800, 600);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);    
    glutCreateWindow("Particle Fountain");
    glutDisplayFunc(renderScene);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutTimerFunc(0, timerFunc, 0);   

    glMatrixMode(GL_PROJECTION);
    gluPerspective(45.0, 1.0, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
    gluLookAt(0.0, 35.0, 25.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);  

    glEnable(GL_DEPTH_TEST);
    lightInit();

    particleList.head = particleList.tail = NULL;
    particleList.size = 0;   

    glutMainLoop();
}
