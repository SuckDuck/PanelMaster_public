#include "dxfout.h"

void DXF_Begin(FILE *F1, DXF_Unit u) {
  fprintf(F1,"999\n");fprintf(F1,"Generated with PanelMaster\n");
  
  fprintf(F1,"0\n");  fprintf(F1,"SECTION\n");
  fprintf(F1,"2\n");  fprintf(F1,"HEADER\n");
  fprintf(F1,"9\n");  fprintf(F1,"$INSUNITS\n");
  fprintf(F1,"70\n%i\n0\n", u);
  fprintf(F1,"ENDSEC\n");

  fprintf(F1,"0\n");  fprintf(F1,"SECTION\n");
  fprintf(F1,"2\n");  fprintf(F1,"ENTITIES\n");
}

void DXF_Point(FILE *F1,char *Layer,int Colour,double X,double Y,double Z) {
  fprintf(F1,"0\n");  fprintf(F1,"POINT\n");            // Point
  fprintf(F1,"8\n");  fprintf(F1,"%s\n",Layer);         // Layer
  fprintf(F1,"62\n"); fprintf(F1,"%d\n",Colour);        // Colour
  fprintf(F1,"10\n"); fprintf(F1,"%0.4lf\n",X);         // X
  fprintf(F1,"20\n"); fprintf(F1,"%0.4lf\n",Y);         // Y
  fprintf(F1,"30\n"); fprintf(F1,"%0.4lf\n",Z);         // Z
}

void DXF_Circle(FILE *F1,char *Layer,int Colour,double X,double Y,double Z,double R) {
  fprintf(F1,"0\n");  fprintf(F1,"CIRCLE\n");           // Circle
  fprintf(F1,"8\n");  fprintf(F1,"%s\n",Layer);         // Layer
  fprintf(F1,"62\n"); fprintf(F1,"%d\n",Colour);        // Colour
  fprintf(F1,"10\n"); fprintf(F1,"%0.4lf\n",X);         // X
  fprintf(F1,"20\n"); fprintf(F1,"%0.4lf\n",Y);         // Y
  fprintf(F1,"30\n"); fprintf(F1,"%0.4lf\n",Z);         // Z
  fprintf(F1,"40\n"); fprintf(F1,"%0.4lf\n",R);         // R
}

void DXF_Line(FILE *F1,char *Layer,int Colour,double X0,double Y0,double Z0,double X1,double Y1,double Z1) {
  fprintf(F1,"0\n");  fprintf(F1,"LINE\n");            // Line
  fprintf(F1,"8\n");  fprintf(F1,"%s\n",Layer);        // Layer
  fprintf(F1,"62\n"); fprintf(F1,"%d\n",Colour);       // Colour
  fprintf(F1,"10\n"); fprintf(F1,"%0.4lf\n",X0);       // X0
  fprintf(F1,"20\n"); fprintf(F1,"%0.4lf\n",Y0);       // Y0
  fprintf(F1,"30\n"); fprintf(F1,"%0.4lf\n",Z0);       // Z0
  fprintf(F1,"11\n"); fprintf(F1,"%0.4lf\n",X1);       // X1
  fprintf(F1,"21\n"); fprintf(F1,"%0.4lf\n",Y1);       // Y1
  fprintf(F1,"31\n"); fprintf(F1,"%0.4lf\n",Z1);       // Z1
}

void DXF_OpenPolylineBegin(FILE *F1,char *Layer,int Colour) {
  fprintf(F1,"0\n");  fprintf(F1,"POLYLINE\n");        // Open polyline
  fprintf(F1,"8\n");  fprintf(F1,"%s\n",Layer);        // Layer
  fprintf(F1,"62\n"); fprintf(F1,"%d\n",Colour);       // Colour
  fprintf(F1,"70\n"); fprintf(F1,"8\n");               // 3D POLYLINE
  fprintf(F1,"66\n"); fprintf(F1,"1\n");               // VERTICES FOLLOW
}

void DXF_OpenPolylinePoint(FILE *F1,double X,double Y,double Z) {
  fprintf(F1,"0\n");  fprintf(F1,"VERTEX\n");          // Vertex (point)
  fprintf(F1,"70\n"); fprintf(F1,"8\n");               // 3D POLYLINE
  fprintf(F1,"10\n"); fprintf(F1,"%0.4lf\n",X);        // X
  fprintf(F1,"20\n"); fprintf(F1,"%0.4lf\n",Y);        // Y
  fprintf(F1,"30\n"); fprintf(F1,"%0.4lf\n",Z);        // Z
}

void DXF_OpenPolylineEnd(FILE *F1) {
  fprintf(F1,"0\n"); fprintf(F1,"SEQEND\n");
}

void DXF_ClosedPolylineBegin(FILE *F1,char *Layer,int Colour) {
  Colour=Colour%16;
  fprintf(F1,"0\n");  fprintf(F1,"POLYLINE\n");        // Closed polyline
  fprintf(F1,"8\n");  fprintf(F1,"%s\n",Layer);        // Layer
  fprintf(F1,"62\n"); fprintf(F1,"%d\n",Colour);       // Colour
  fprintf(F1,"70\n"); fprintf(F1,"9\n");               // CLOSED 3D POLYLINE (SHAPE)
  fprintf(F1,"66\n"); fprintf(F1,"1\n");               // VERTICES FOLLOW
}

void DXF_ClosedPolylinePoint(FILE *F1,double X,double Y,double Z) {
  fprintf(F1,"0\n");  fprintf(F1,"VERTEX\n");          // Vertex (point)
  fprintf(F1,"70\n"); fprintf(F1,"9\n");               // CLOSED 3D POLYLINE (SHAPE)
  fprintf(F1,"10\n"); fprintf(F1,"%0.4lf\n",X);        // X
  fprintf(F1,"20\n"); fprintf(F1,"%0.4lf\n",Y);        // Y
  fprintf(F1,"30\n"); fprintf(F1,"%0.4lf\n",Z);        // Z
}

void DXF_ClosedPolylineEnd(FILE *F1) {
  fprintf(F1,"0\n"); fprintf(F1,"SEQEND\n");
}

void DXF_End(FILE *F1) {
  fprintf(F1,"0\n"); fprintf(F1,"ENDSEC\n");
  fprintf(F1,"0\n"); fprintf(F1,"EOF");
}
