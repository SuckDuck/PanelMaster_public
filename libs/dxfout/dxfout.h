#ifndef DXFOUT_H_INCLUDED
#define DXFOUT_H_INCLUDED

#include "stdio.h"
#include <stdio.h>

typedef enum DXF_Unit {
	DXF_UNITLESS,
	DXF_INCHES,
	DXF_FEET,
	DXF_MILES,
	DXF_MILLIMETERS,
	DXF_CENTIMETERS,
	DXF_METERS,
	DXF_KILOMETERS
} DXF_Unit;

typedef struct DXF {
	FILE *f;
	char *layer;
	int color;
} DXF;

/* Prototypes */
void DXF_Begin(FILE *F1, DXF_Unit u);

void DXF_Point(FILE *F1,char *Layer,int Colour,double X,double Y,double Z);
void DXF_Circle(FILE *F1,char *Layer,int Colour,double X,double Y,double Z,double R);
void DXF_Line(FILE *F1,char *Layer,int Colour,double X0,double Y0,double Z0,double X1,double Y1,double Z1);

void DXF_OpenPolylineBegin(FILE *F1,char *Layer,int Colour);
void DXF_OpenPolylinePoint(FILE *F1,double X,double Y,double Z);
void DXF_OpenPolylineEnd(FILE *F1);

void DXF_ClosedPolylineBegin(FILE *F1,char *Layer,int Colour);
void DXF_ClosedPolylinePoint(FILE *F1,double X,double Y,double Z);
void DXF_ClosedPolylineEnd(FILE *F1);

void DXF_End(FILE *F1);

#endif