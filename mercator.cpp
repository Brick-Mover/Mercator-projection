//
//  camera.cpp
//  Fisheye Camera
//
//  Created by SHAO Jiuru on 6/25/16.
//  Copyright © 2016 UCLA. All rights reserved.
//


#include "mercator.h"
#include "matm.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string.h>
#include <vector>
#include <math.h>
#include <cstdlib>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace std;


void eat_comment(ifstream &f);
void load_ppm(Image* img, const string &name);
void saveImg(vector<Pixel> pic, int length, int height);
void initializeWalls(Image* pic);


int main(int argc, const char * argv[])
{
    Image* s = new Image(2058, 1746);
    initializeWalls(s);
    
    Fisheye* f = new Fisheye(M_PI ,0,origin,s,600,600);
    f->render();
    vector<Pixel> result = f->getImage();
    saveImg(result, 600, 600);
}


Fisheye::Fisheye(float aperture,
                 float viewAngle,
                 Point cPos,
                 Image* image,
                 int xDim, int yDim): cameraPos(cPos)
{
    this->aperture = aperture;
    this->xDim = xDim;
    this->yDim = yDim;
    this->image = image;
    imagePlane.reserve(yDim*xDim);
    rotationM = mat3(
                vec3(cosf(viewAngle), -sinf(viewAngle), 0),
                vec3(sinf(viewAngle),  cosf(viewAngle), 0),
                vec3(0.0, 0.0, 1.0));
}


void Fisheye::render()
{
    // On view plane, render each pixel
    for (int r = 0; r < yDim; r++) {
        for (int c = 0; c < xDim; c++) {
            renderPixel(r, c);
        }
    }
//    renderPixel(388, 26);
}


void Fisheye::renderPixel(int row, int col)
{
    // normalize (r,c) to coordinate [-1, 1]
    float xn = 2*float(col)/xDim - 1;
    float yn = 1 - 2*float(row)/yDim;
    float r = sqrtf(xn*xn + yn*yn);
    if (r > 1) {
        setColor(row, col, grey_pixel);
        return;
    }
    // get Cartesian coordinate on the sphere
    // theta: [0,2π] (xy-plane)
    float theta = atan2f(yn, xn);
    // phi: [0,π/2] (z direction)
    float phi = r * aperture / 2;
    
    float x = cosf(theta) * sinf(phi);
    float y = sinf(theta) * sinf(phi);
    float z = cosf(phi);
    
    float xsphere = z;
    float ysphere = -x;
    float zsphere = y;
    
    /* NOTE: For UV mapping, use the following coordinate:
    float u = atan2(ysphere, xsphere)/aperture;
    float v = (asin(zsphere) + M_PI/2)/M_PI;
    setColor(row, col, image->getColor(u, v));
    */
    
    float lambda = atan2f(ysphere, xsphere);
    if (lambda < 0)
        lambda += 2*M_PI;
    lambda = -lambda;
    float fi = -acos(zsphere) + M_PI_2;
    
    int px = int(image->R * lambda);
    int py = int(image->R * log(tan(M_PI_4 + fi/2)));

    if (py > image->yDim/2 || py <= -image->yDim/2) {
        setColor(row, col, grey_pixel);
        return;
    }

    
    setColor(row, col, image->getColor(px, py));
    

}


Pixel Image::getColor(int x, int y)
{
    int r = yDim/2 - y;
    int c = x + xDim/2;
    assert(0 <= r*xDim+c <= xDim*yDim-1);
    return pixels[r*xDim+c];
    /* Note: for UV mapping, the the following code: 
    Pixel Image::getColor(float u, float v)
{
    int x = u*xDim;
    int y = v*yDim;
    int r = yDim - y;
    int c = xDim - x;
    assert(0 <= r*xDim+c <= xDim*yDim-1);
    return pixels[r*xDim+c];
}

    */
}


void Fisheye::setColor(int row, int col, Pixel color)
{
    assert(0 <= row*xDim+col <= xDim*yDim-1);
    imagePlane.push_back(color);
}



/* trivial member functions such as assign overloading and constructors */
Point& Point::operator = (Point other)
{
    x = other.x;
    y = other.y;
    z = other.z;
    return *this;
}


bool Point::operator == (Point other)
{
    return other.x == this->x &&
        other.y == this->y &&
        other.z == this->z;
}


Pixel& Pixel::operator = (Pixel other)
{
    R = other.R;
    G = other.G;
    B = other.B;
    return *this;
}




/* BELOW: load and save images */
void saveImg(vector<Pixel> pic, int length, int height)
{
    FILE *fp = fopen("result.ppm", "wb");
    (void) fprintf(fp, "P6\n%d %d\n255\n", length, height);
    int count = 0;
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < length; ++i)
        {
            static unsigned char color[3];
            color[0] = pic[count].R;  /* red */
            color[1] = pic[count].G;  /* green */
            color[2] = pic[count].B;  /* blue */
            (void) fwrite(color, 1, 3, fp);
            count++;
        }
    }
    (void) fclose(fp);
}


void initializeWalls(Image* test)
{
    load_ppm(test, "test.ppm");
}


void eat_comment(ifstream &f)
{
    char linebuf[1024];
    char ppp;
    while (ppp = f.peek(), ppp == '\n' || ppp == '\r')
        f.get();
    if (ppp == '#')
        f.getline(linebuf, 1023);
}


void load_ppm(Image* img, const string &name)
{
    ifstream f(name.c_str(), ios::binary);
    if (f.fail())
    {
        cout << "Could not open file: " << name << endl;
        return;
    }
    
    // get type of file
    eat_comment(f);
    int mode = 0;
    string s;
    f >> s;
    if (s == "P3")
        mode = 3;
    else if (s == "P6")
        mode = 6;
    
    // get w
    eat_comment(f);
    f >> img->xDim;
    
    // get h
    eat_comment(f);
    f >> img->yDim;
    
    // get bits
    eat_comment(f);
    int bits = 0;
    f >> bits;
    
    // error checking
    if (mode != 3 && mode != 6)
    {
        cout << "Unsupported magic number" << endl;
        f.close();
        return;
    }
    if (img->xDim < 1)
    {
        cout << "Unsupported width: " << img->xDim << endl;
        f.close();
        return;
    }
    if (img->yDim < 1)
    {
        cout << "Unsupported height: " << img->yDim << endl;
        f.close();
        return;
    }
    if (bits < 1 || bits > 255)
    {
        cout << "Unsupported number of bits: " << bits << endl;
        f.close();
        return;
    }
    
    // load image data
    img->pixels.resize(img->xDim * img->yDim);
    
    if (mode == 6)
    {
        f.get();
        f.read((char*)&img->pixels[0], img->pixels.size() * 3);
    }
    else if (mode == 3)
    {
        for (int i = 0; i < img->pixels.size(); i++)
        {
            int v;
            f >> v;
            img->pixels[i].R = v;
            f >> v;
            img->pixels[i].G = v;
            f >> v;
            img->pixels[i].B = v;
        }
    }
    
    // close file
    f.close();
}

