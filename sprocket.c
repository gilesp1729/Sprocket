#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <windows.h>

#include "resource.h"
#include "dxf.h"

typedef struct
{
    double  x;
    double  y;
}
    Tooth;

/*
 * Globals.
 */
HINSTANCE   hInst;		/* Instance thingy */
int	    xoff, yoff;		/* Offset values for printing pages */
FILE	    *dxf;		/* DXF file to save to */

/*
 * Input data values.
 */
int	    nchain = 6;		/* ANSI 08A - standard bike chain */
int	    n = 40;		/* Number of teeth */
double	    ecc = 0;		/* Eccentricity (% major over minor) */
double	    pcd = 110;		/* PCD mm of bolt circle */
double	    mounting = 94;	/* Diameter of mounting circle */
double	    crank = 0;		/* Angle crank to major axis */
double	    web = 15;		/* Thickness of web left by scallop */
double	    bolt_hole = 10;	/* Size of bolt holes */
int	    nholes = 5;		/* Number of them */
double      extra_rad = 0.2;    /* Extra radius on tooth circle for clearance */
int         layout_lines = 1;   /* Include layout lines and punch marks? */

/*
 * Derived values.
 */
double	    a, b;		/* Derived major and minor axes */
double	    r;			/* Radius (a = b = r for a circle) */
Tooth	    *tooth = NULL;	/* Array of tooth coordinates */

/*
 * Standard chain pitches and roller dimensions.
 */
#define NCHAINS	    12

struct
{
    char    name[48];
    double  pitch;
    double  roller;
}
    chain[NCHAINS] =
{
    {"6mm x 4mm     (BS/DIN 04B)", 6.00,  4.00},
    {"1/4 x 3.3mm   (ANSI 03C)",   6.35,  3.30},
    {"8mm x 5mm     (BS/DIN 05B)", 8.00,  5.00},
    {"3/8 x 5.08mm  (ANSI 06C)",   9.525, 5.08},
    {"3/8 x 1/4     (BS/DIN 06B)", 9.525, 6.35},
    {"1/2 x 7.77mm  (ANSI 085)",   12.7,  7.77},
    {"1/2 x 7.95mm  (ANSI 08A, bicycle)", 12.7, 7.95},   /* standard bicycle chain */
    {"1/2 x 8.5mm   (BS/DIN 08B)", 12.7,  8.51},
    {"5/8 x 10.16mm (ANSI 10A)",   15.875,10.16},
    {"5/8 x 10.16mm (BS/DIN 10B)", 15.875,10.16},
    {"3/4 x 11.91mm (ANSI 12A)",   19.05, 11.91},
    {"3/4 x 12.07mm (BS/DIN 12B)", 19.05, 12.07}
};


#define	    PI	    3.1415926535897932
#define	    RAD	    57.29577951308232

/*
 * The tolerance on a tooth step, in mm
 */
#define TOL	    1.0e-5

/*
 * The radius of the center punch mark, in mm.
 */
#define PUNCH	    0.3

/*
 * The overlap between pages, in mm.
 */
#define OVERLAP	    15

 
/*
 * Work out some initial settings.
 */
void
calc_initial(void)
{
    double  e;

    r = (n * chain[nchain].pitch) / (2 * PI);
    e = sqrt(1 + ecc / 100.0);
    a = r * e;
    b = r / e;

    if (tooth != NULL)
	free(tooth);
    tooth = malloc((n + 1) * sizeof(Tooth));
}


/*
 * Given a current (tx, ty) and a previous (prevx, prevy)
 * step to the next tooth.
 */
void
next_tooth
(
    double  tx, 
    double  ty, 
    double  prevx, 
    double  prevy, 
    double  *nextx,
    double  *nexty
)
{
    double  x, y, step;
    double  a2, b2, x2, y2;
    double  A, B, C;
    double  dx, dy, dist;
    double  xpos, xneg, ypos, yneg;

    step = 1.0;
    a2 = a * a;
    b2 = b * b;
    
    while (TRUE)
    {
	x = tx + step * (tx - prevx);
	y = ty + step * (ty - prevy);

	/*
	 * For numerical stability, split into two depending on
	 * the quadrant.
	 */
	if (fabs(tx) > fabs(ty))
	{
	    /*
	     * Calculate x given y.
	     *
	     * Equation of the ellipse is:
	     *
	     * x^2/a^2 + y^2/b^2 = 1
	     *
	     * Given a known y, this becomes a quadratic in x:
	     *
	     * x^2/a^2 + y^2/b^2 - 1 = 0
	     *
	     * We want the solution nearest to tx.
	     * In general the solutions will have opposite signs.
	     */
	    if (y > b)
		y = b;
	    else if (y < -b)
		y = -b;
	    y2 = y * y;
	    A = 1.0 / a2;
	    B = 0;
	    C = (y2 / b2) - 1;
	    xpos = (-B + sqrt((B * B) - 4 * A * C)) / (2 * A);
	    xneg = (-B - sqrt((B * B) - 4 * A * C)) / (2 * A);
	    if (fabs(xpos - tx) < fabs(xneg - tx))
		x = xpos;
	    else
		x = xneg;
	}
	else
	{
	    /*
	     * Calculate y given x.
	     *
	     * Given a known x, this becomes a quadratic in y:
	     *
	     * y^2/b^2 + x^2/a^2 - 1 = 0
	     *
	     * We want the solution nearest to ty.
	     */
	    if (x > a)
		x = a;
	    else if (x < -a)
		x = -a;
	    x2 = x * x;
	    A = 1 / b2;
	    B = 0;
	    C = x2 / a2 - 1;
	    ypos = (-B + sqrt((B * B) - 4 * A * C)) / (2 * A);
	    yneg = (-B - sqrt((B * B) - 4 * A * C)) / (2 * A);
	    if (fabs(ypos - ty) < fabs(yneg - ty))
		y = ypos;
	    else
		y = yneg;
	}

	/*
	 * Calculate the distance between (tx, ty) and (x, y).
	 * This will typically be not quite TOOTH. Apply any error
	 * factor to the original step and recalculate.
	 */
	dx = tx - x;
	dy = ty - y;
	dist = sqrt((dx * dx) + (dy * dy));
	if (fabs(dist - chain[nchain].pitch) < TOL)
	    break;

	step *= chain[nchain].pitch / dist;
    }

    *nextx = x;
    *nexty = y;
}


/* 
 * Calculate a sprocket.
 */
void
calc_sprocket()
{
    int	    i;
    double  total;
    double  error;
    double  prevx, prevy;

    while(TRUE)
    {
	prevx = a;
	prevy = -chain[nchain].pitch;
	tooth[0].x = a;
	tooth[0].y = 0;

	/*
	 * Generate tooth[1] to [n]
	 */
	for (i = 0; i < n; i++)
	{
	    next_tooth
	    (
		tooth[i].x, tooth[i].y,
		prevx, prevy,
		&tooth[i+1].x, &tooth[i+1].y
	    );

	    prevx = tooth[i].x;
	    prevy = tooth[i].y;
	}

	/*
	 * We should end up near (a, 0). Calculate the
	 * fractional difference to apply to a and b to correct
	 * for the error. Assume the line is roughly vertical here.
	 */
	total = n * chain[nchain].pitch;
	error = (total + tooth[n].y) / total;
	if (fabs(error - 1.0) < TOL)
	    break;

	a *= error;
	b *= error;
    }
}


/*
 * Local radius of ellipse given angle in radians.
 */
double
local_rad(double angle)
{
    double  cosa = cos(angle);
    double  sina = sin(angle);
    double  cos2a = cosa * cosa;
    double  sin2a = sina * sina;
    double  a2 = a * a;
    double  b2 = b * b;
    double  rad;

    rad = sqrt(a2 * b2 / (b2 * cos2a + a2 * sin2a));
    return rad;
}


/*
 * Mapping from mm to plot space.
 */
#define MARG	20.0
#define X(x)	(int)(((x) + a + MARG - xoff) * 10.0)
#define Y(y)	(int)((-b - MARG + yoff + (y)) * 10.0)

#define CIRCLE(hdc,x,y,r)   Arc(hdc,X(x-r),Y(y-r),X(x+r),Y(y+r),X(x+r),Y(y+r),X(x+r),Y(y+r))

/*
 * Draw the sprocket.
 */
void
plot(HDC hdc)
{
    int	    i, j;
    double  x, y;
    double  x1, y1;
    double  x2, y2;
    double  x3, y3;
    double  angle, delta, da;
    double  lr2, lr3;
    char    string[255];
    
    SetMapMode(hdc, MM_LOMETRIC);

    if (layout_lines)
    {
        /*
         * Draw axes.
         */
        MoveToEx(hdc, X(-chain[nchain].roller), Y(0), NULL);
        LineTo(hdc, X(chain[nchain].roller), Y(0));
        MoveToEx(hdc, X(0), Y(-chain[nchain].roller), NULL);
        LineTo(hdc, X(0), Y(chain[nchain].roller));

        /*
         * Draw crank direction.
         */
        x = (mounting / 2) * cos(crank / RAD);
        y = (mounting / 2) * sin(crank / RAD);
        MoveToEx(hdc, X(0), Y(0), NULL);
        LineTo(hdc, X(x), Y(y));
        CIRCLE(hdc, x, y, chain[nchain].roller/2);
    }

    /*
     * Print info in top left corner.
     */
    if (xoff == 0 && yoff == 0)
    {
	sprintf(string, "Teeth: %d  Chain: %s", n, chain[nchain].name);
	TextOut(hdc, 5, 0, string, lstrlen(string));
	sprintf(string, "Drill %gmm (%g inch) holes", 
                (chain[nchain].roller + extra_rad*2), 
                (chain[nchain].roller  + extra_rad*2) / 25.4);
	TextOut(hdc, 5, -50, string, lstrlen(string));
	sprintf(string, "PCD: %g mm on %g mm circle", pcd, mounting);
	TextOut(hdc, 5, -100, string, lstrlen(string));
	sprintf(string, "Crank at %g degrees", crank);
	TextOut(hdc, 5, -150, string, lstrlen(string));
	if (ecc > 0)
	{
	    sprintf(string, "Ecc %g%%", ecc);
	    TextOut(hdc, 5, -200, string, lstrlen(string));
	}
    }

    SelectObject(hdc, GetStockObject(BLACK_PEN));

    /*
     * Punch marks for teeth drillings.
     */
    if (layout_lines)
    {    
	for (i = 0; i < n; i++)
            CIRCLE(hdc, tooth[i].x, tooth[i].y, PUNCH);
    }

    /*
     * Draw tooth drillings.
     */
    for (i = n-1; i >= 0; i--)
    {
        double  rad = chain[nchain].roller/2 + extra_rad;
        double  trad = chain[nchain].pitch - rad;
        double  frac = rad / chain[nchain].pitch;
        int     prev = (i == 0) ? n - 1 : i - 1;
        int     next = (i == n - 1) ? 0 : i + 1;
        double  halfpitch = chain[nchain].pitch/2;
        double  xtra = 2 * sqrt(trad*trad - halfpitch*halfpitch);
        double  theta, dropx, dropy;

        theta = atan2(tooth[i].x - tooth[next].x, tooth[next].y - tooth[i].y);
        dropx = 0.3 * rad * cos(theta);
        dropy = 0.3 * rad * sin(theta);
        x2 = tooth[i].x + frac * (tooth[next].x - tooth[i].x) - dropx;
        y2 = tooth[i].y + frac * (tooth[next].y - tooth[i].y) - dropy;
        x1 = tooth[i].x + xtra * cos(theta) - dropx;
        y1 = tooth[i].y + xtra * sin(theta) - dropy;

	if (i == n-1)
	    MoveToEx(hdc, X(x2), Y(y2), NULL);

	SetArcDirection(hdc, AD_CLOCKWISE);
        ArcTo
        (
            hdc, 
            X(tooth[next].x - trad - dropx),
            Y(tooth[next].y - trad - dropy),
            X(tooth[next].x + trad - dropx),
            Y(tooth[next].y + trad - dropy),
            X(x1), Y(y1),
            X(x2 + dropx), Y(y2 + dropy)
        );

        theta = atan2(tooth[prev].x - tooth[i].x, tooth[i].y - tooth[prev].y);
        dropx = 0.3 * rad * cos(theta);
        dropy = 0.3 * rad * sin(theta);
        x3 = tooth[i].x + frac * (tooth[prev].x - tooth[i].x) - dropx;
        y3 = tooth[i].y + frac * (tooth[prev].y - tooth[i].y) - dropy;
        x1 = tooth[i].x + xtra * cos(theta) - dropx;
        y1 = tooth[i].y + xtra * sin(theta) - dropy;

	SetArcDirection(hdc, AD_COUNTERCLOCKWISE);
        ArcTo
        (
            hdc, 
            X(tooth[i].x - rad),
            Y(tooth[i].y - rad),
            X(tooth[i].x + rad),
            Y(tooth[i].y + rad),
            X(x2), Y(y2),
            X(x3), Y(y3)
        );

	SetArcDirection(hdc, AD_CLOCKWISE);
        ArcTo
        (
            hdc, 
            X(tooth[prev].x - trad - dropx),
            Y(tooth[prev].y - trad - dropy),
            X(tooth[prev].x + trad - dropx),
            Y(tooth[prev].y + trad - dropy),
            X(x3 + dropx), Y(y3 + dropy),
            X(x1), Y(y1)
        );
    }
    
    SetArcDirection(hdc, AD_COUNTERCLOCKWISE);

    /*
     * Draw mounting circle and crank bolt holes.
     * The crank points in between two of the bolt holes.
     * If there are no holes, just draw the mounting circle.
     */
    if (nholes > 0)
    {
	angle = 360.0 / nholes;
	j = 0;
	for 
	(
	    i = 0, delta = crank + angle / 2; 
	    i < nholes; 
	    i++, delta += angle
	)
	{
	    x = pcd * cos(delta / RAD) / 2;
	    y = pcd * sin(delta / RAD) / 2;
            if (layout_lines)
	        CIRCLE(hdc, x, y, PUNCH);
	    CIRCLE(hdc, x, y, bolt_hole / 2);

	    /*
	     * Draw segments of mounting circle next to bolt holes.
	     * If there is room, draw scallops in between the holes.
	     */
	    da = 2 * bolt_hole / mounting;
	    x1 = mounting * cos(delta / RAD - da) / 2;
	    y1 = mounting * sin(delta / RAD - da) / 2;
	    x2 = mounting * cos(delta / RAD + da) / 2;
	    y2 = mounting * sin(delta / RAD + da) / 2;
	    x3 = mounting * cos((delta + angle) / RAD - da) / 2;     /* next hole */
	    y3 = mounting * sin((delta + angle) / RAD - da) / 2;
	    Arc
	    (
		hdc, 
		X(-mounting/2), 
		Y(-mounting/2),
		X(mounting/2), 
		Y(mounting/2),
		X(x1), Y(y1),
		X(x2), Y(y2)
	    );

	    /*
	     * Calculate the local radius at points 2 and 3. Leave some
	     * room so there is metal outside the scallops.
	     *
	     * Calculate points 2a, 2b, 3b, 3a. Make the control points 
	     * for 2 beziers: 2, 2a, 2b, (2b+3b)/2, 3b, 3a, 3.
	     */
	    lr2 = local_rad(delta / RAD + da) - web;
	    lr3 = local_rad((delta + angle) / RAD - da) - web;

	    /*
	     * Draw scallops, if there is room, and we have enough holes.
	     */
	    if (lr2 > mounting / 2 && lr3 > mounting / 2 && nholes >= 3)
	    {
		double  lr2b = local_rad((delta + angle / 3) / RAD) - web;
		double  lr3b = local_rad((delta + 2 * angle / 3) / RAD) - web;
		double  x2a = lr2 * cos(delta / RAD + da);
		double  y2a = lr2 * sin(delta / RAD + da);
		double  x2b = lr2b * cos((delta + angle / 3) / RAD);
		double  y2b = lr2b * sin((delta + angle / 3) / RAD);
		double  x3b = lr3b * cos((delta + 2 * angle / 3) / RAD);
		double  y3b = lr3b * sin((delta + 2 * angle / 3) / RAD);
		double  x3a = lr3 * cos((delta + angle) / RAD - da);
		double  y3a = lr3 * sin((delta + angle) / RAD - da);
		POINT   pts[7] =
		{
		    {X(x2), Y(y2)},
		    {X(x2a), Y(y2a)},
		    {X(x2b), Y(y2b)},
		    {X((x2b + x3b) / 2), Y((y2b + y3b) / 2)},  /* collinear */
		    {X(x3b), Y(y3b)},
		    {X(x3a), Y(y3a)},
		    {X(x3), Y(y3)}
		};

		PolyBezier(hdc, pts, 7);
	    }
	    else   /* just the circle */
	    {
		Arc
		(
		    hdc, 
		    X(-mounting/2), 
		    Y(-mounting/2),
		    X(mounting/2), 
		    Y(mounting/2),
		    X(x2), Y(y2),
		    X(x3), Y(y3)
		);
	    }
	}
    }
    else
    {
	CIRCLE(hdc, 0, 0, mounting / 2);
    }
}


/*
 * Draw the sprocket to a DXF file.
 */
void
save_dxf(FILE *file)
{
    int	    i, j;
    double  x, y;
    double  x1, y1;
    double  x2, y2;
    double  x3, y3;
    double  angle, delta, da;
    double  lr2, lr3;
    
    if (layout_lines)
    {
        /*
         * Draw axes.
         */
        dxf_line(file, -chain[nchain].roller, 0, chain[nchain].roller, 0);
        dxf_line(file, 0, -chain[nchain].roller, 0, chain[nchain].roller);

        /*
         * Draw crank direction.
         */
        x = (mounting / 2) * cos(crank / RAD);
        y = (mounting / 2) * sin(crank / RAD);
        dxf_line(file, 0, 0, x, y);
        dxf_circle(file, x, y, chain[nchain].roller/2);
    }

    /*
     * Punch marks for teeth drillings.
     */
    if (layout_lines)
    {    
	for (i = 0; i < n; i++)
 	    dxf_circle(file, tooth[i].x, tooth[i].y, PUNCH);
    }

    dxf_polyline_start(file);

    /*
     * Draw tooth drillings.
     */
    for (i = n-1; i >= 0; i--)
    {
        double  rad = chain[nchain].roller/2 + extra_rad;
        double  trad = chain[nchain].pitch - rad;
        double  frac = rad / chain[nchain].pitch;
        int     prev = (i == 0) ? n - 1 : i - 1;
        int     next = (i == n - 1) ? 0 : i + 1;
        double  halfpitch = chain[nchain].pitch/2;
        double  xtra = 2 * sqrt(trad*trad - halfpitch*halfpitch);
        double  theta, dropx, dropy;

        theta = atan2(tooth[i].x - tooth[next].x, tooth[next].y - tooth[i].y);
        dropx = 0.3 * rad * cos(theta);
        dropy = 0.3 * rad * sin(theta);
        x2 = tooth[i].x + frac * (tooth[next].x - tooth[i].x) - dropx;
        y2 = tooth[i].y + frac * (tooth[next].y - tooth[i].y) - dropy;
        x1 = tooth[i].x + xtra * cos(theta) - dropx;
        y1 = tooth[i].y + xtra * sin(theta) - dropy;

        dxf_arc_to
        (
            file, 
            tooth[next].x - trad - dropx,
            tooth[next].y - trad - dropy,
            tooth[next].x + trad - dropx,
            tooth[next].y + trad - dropy,
            x1, y1,
            x2 + dropx, y2 + dropy,
	    1
        );

        theta = atan2(tooth[prev].x - tooth[i].x, tooth[i].y - tooth[prev].y);
        dropx = 0.3 * rad * cos(theta);
        dropy = 0.3 * rad * sin(theta);
        x3 = tooth[i].x + frac * (tooth[prev].x - tooth[i].x) - dropx;
        y3 = tooth[i].y + frac * (tooth[prev].y - tooth[i].y) - dropy;
        x1 = tooth[i].x + xtra * cos(theta) - dropx;
        y1 = tooth[i].y + xtra * sin(theta) - dropy;

        dxf_arc_to
        (
            file, 
            tooth[i].x - rad,
            tooth[i].y - rad,
            tooth[i].x + rad,
            tooth[i].y + rad,
            x2, y2,
            x3, y3,
	    0
        );

        dxf_arc_to
        (
            file, 
            tooth[prev].x - trad - dropx,
            tooth[prev].y - trad - dropy,
            tooth[prev].x + trad - dropx,
            tooth[prev].y + trad - dropy,
            x3 + dropx, y3 + dropy,
            x1, y1,
	    1
        );
    }
    
    dxf_polyline_finish(file);

    /*
     * Draw mounting circle and crank bolt holes.
     * The crank points in between two of the bolt holes.
     * If there are no holes, just draw the mounting circle.
     */
    if (nholes > 0)
    {
	angle = 360.0 / nholes;
	j = 0;
	for 
	(
	    i = 0, delta = crank + angle / 2; 
	    i < nholes; 
	    i++, delta += angle
	)
	{
	    x = pcd * cos(delta / RAD) / 2;
	    y = pcd * sin(delta / RAD) / 2;
            if (layout_lines)
	        dxf_circle(file, x, y, PUNCH);
	    dxf_circle(file, x, y, bolt_hole / 2);

	    /*
	     * Draw segments of mounting circle next to bolt holes.
	     * If there is room, draw scallops in between the holes.
	     */
	    da = 2 * bolt_hole / mounting;
	    x1 = mounting * cos(delta / RAD - da) / 2;
	    y1 = mounting * sin(delta / RAD - da) / 2;
	    x2 = mounting * cos(delta / RAD + da) / 2;
	    y2 = mounting * sin(delta / RAD + da) / 2;
	    x3 = mounting * cos((delta + angle) / RAD - da) / 2;     /* next hole */
	    y3 = mounting * sin((delta + angle) / RAD - da) / 2;
	    dxf_arc
	    (
		file, 
		-mounting/2, 
		-mounting/2,
		mounting/2, 
		mounting/2,
		x1, y1,
		x2, y2
	    );

	    /*
	     * Calculate the local radius at points 2 and 3. Leave some
	     * room so there is metal outside the scallops.
	     *
	     * Calculate points 2a, 2b, 3b, 3a. Make the control points 
	     * for 2 beziers: 2, 2a, 2b, (2b+3b)/2, 3b, 3a, 3.
	     */
	    lr2 = local_rad(delta / RAD + da) - web;
	    lr3 = local_rad((delta + angle) / RAD - da) - web;

	    /*
	     * Draw scallops, if there is room, and we have enough holes.
	     */
	    if (lr2 > mounting / 2 && lr3 > mounting / 2 && nholes >= 3)
	    {
		double  lr2b = local_rad((delta + angle / 3) / RAD) - web;
		double  lr3b = local_rad((delta + 2 * angle / 3) / RAD) - web;
		double  x2a = lr2 * cos(delta / RAD + da);
		double  y2a = lr2 * sin(delta / RAD + da);
		double  x2b = lr2b * cos((delta + angle / 3) / RAD);
		double  y2b = lr2b * sin((delta + angle / 3) / RAD);
		double  x3b = lr3b * cos((delta + 2 * angle / 3) / RAD);
		double  y3b = lr3b * sin((delta + 2 * angle / 3) / RAD);
		double  x3a = lr3 * cos((delta + angle) / RAD - da);
		double  y3a = lr3 * sin((delta + angle) / RAD - da);
                double xarray[7] =
		{
		    x2,
		    x2a,
		    x2b,
		    (x2b + x3b) / 2,  /* collinear */
		    x3b,
		    x3a,
		    x3
		};
                double yarray[7] =
		{
		    y2,
		    y2a,
		    y2b,
		    (y2b + y3b) / 2,  /* collinear */
		    y3b,
		    y3a,
		    y3
		};

                dxf_polybezier(file, 7, xarray, yarray);
	    }
	    else   /* just the circle */
	    {
		dxf_arc
		(
		    file, 
		    -mounting/2, 
		    -mounting/2,
		    mounting/2, 
		    mounting/2,
		    x2, y2,
		    x3, y3
		);
	    }
	}
    }
    else
    {
	dxf_circle(file, 0, 0, mounting / 2);
    }
}


/*
 * Settings dialog procedure.
 */
int WINAPI
SettingsDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    char    string[40];
    char    *end;
    double  temp;
    int	    ctrl;
    int	    ntemp;
    BOOL    ok;
    char    *err;
    int	    i;

    static int	    saved_nchain;
    static int	    saved_n;
    static double   saved_ecc;
    static double   saved_pcd;
    static double   saved_bolt_hole;
    static int	    saved_nholes;
    static double   saved_mounting;
    static double   saved_crank;
    static double   saved_web;
    static double   saved_extra;
    static RECT	    wr = {20, 20, 0, 0};
    static int	    focus = IDC_TEETH;
    static int	    saved_layout;

    switch (msg)
    {
    case WM_INITDIALOG:
	SetWindowText(hWnd, "Sprocket settings");
	SetWindowPos(hWnd, NULL, wr.left, wr.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	SetFocus(GetDlgItem(hWnd, focus));
	SendDlgItemMessage(hWnd, focus, EM_SETSEL, 0, -1);

	saved_nchain = nchain;
	saved_n = n;
	saved_ecc = ecc;
	saved_pcd = pcd;
	saved_bolt_hole = bolt_hole;
	saved_nholes = nholes;
	saved_mounting = mounting;
	saved_crank = crank;
	saved_web = web;
        saved_layout = layout_lines;
        saved_extra = extra_rad;

	SetDlgItemInt(hWnd, IDC_TEETH, n, TRUE);
	sprintf(string, "%g", ecc);
	SetDlgItemText(hWnd, IDC_ECC, string);
	sprintf(string, "%g", pcd);
	SetDlgItemText(hWnd, IDC_PCD, string);
	SetDlgItemInt(hWnd, IDC_N_HOLES, nholes, TRUE);
	sprintf(string, "%g", bolt_hole);
	SetDlgItemText(hWnd, IDC_BOLT_HOLE, string);
	sprintf(string, "%g", extra_rad);
	SetDlgItemText(hWnd, IDC_EXTRA, string);
	sprintf(string, "%g", mounting);
	SetDlgItemText(hWnd, IDC_MOUNTING, string);
	sprintf(string, "%g", crank);
	SetDlgItemText(hWnd, IDC_CRANK, string);
	sprintf(string, "%g", web);
	SetDlgItemText(hWnd, IDC_WEB, string);

	SetDlgItemText(hWnd, IDS_ERR, "");

	for (i = 0; i < NCHAINS; i++)
	    SendDlgItemMessage
	    (
		hWnd, 
		IDC_CHAIN, 
		CB_INSERTSTRING, 
		i, 
		(LPARAM)(LPCTSTR)chain[i].name
	    );

	SendDlgItemMessage(hWnd, IDC_CHAIN, CB_SETCURSEL, nchain, 0);
        SendDlgItemMessage(hWnd, IDC_LAYOUT, BM_SETCHECK, layout_lines ? BST_CHECKED : BST_UNCHECKED, 0);

	return FALSE;

    case WM_MOVE:		    /* For poor man's modeless dialog */
	GetWindowRect(hWnd, &wr);   /* lParam not useful!! */
	break;

    case WM_COMMAND:
	switch (LOWORD(wParam))
	{
	case IDOK:
	case IDAPPLY:
	    nchain = SendDlgItemMessage(hWnd, IDC_CHAIN, CB_GETCURSEL, 0, 0);
            layout_lines = SendDlgItemMessage(hWnd, IDC_LAYOUT, BM_GETSTATE, 0, 0) == BST_CHECKED;

	    ctrl = IDC_TEETH;
	    err = "Number of teeth must be at least 12";
	    ntemp = GetDlgItemInt(hWnd, ctrl, &ok, TRUE);
	    if (!ok || ntemp < 12)
		goto beep;
	    n = ntemp;

	    ctrl = IDC_ECC;
	    err = "Eccentricity must be between 0 and 300%";
	    GetDlgItemText(hWnd, ctrl, string, sizeof(string));
	    temp = strtod(string, &end);
	    if (temp < 0.0 || temp > 300 || *end != '\0')
		goto beep;
	    ecc = temp;

	    ctrl = IDC_PCD;
	    err = "PCD must be greater than 0";
	    GetDlgItemText(hWnd, ctrl, string, sizeof(string));
	    if ((temp = strtod(string, &end)) < 0.0 || *end != '\0')
		goto beep;
	    pcd = temp;

	    ctrl = IDC_BOLT_HOLE;
	    err = "Bolt hole size must be greater than 0";
	    GetDlgItemText(hWnd, ctrl, string, sizeof(string));
	    if ((temp = strtod(string, &end)) < 0.0 || *end != '\0')
		goto beep;
	    bolt_hole = temp;

	    ctrl = IDC_EXTRA;
	    err = "Extra radius on bolt hole must non-negative";
	    GetDlgItemText(hWnd, ctrl, string, sizeof(string));
	    if ((temp = strtod(string, &end)) <= 0.0 || *end != '\0')
		goto beep;
	    extra_rad = temp;

	    ctrl = IDC_N_HOLES;
	    err = "Number of holes must be greater than 0";
	    ntemp = GetDlgItemInt(hWnd, ctrl, &ok, TRUE);
	    if (!ok || ntemp < 0)
		goto beep;
	    nholes = ntemp;

	    ctrl = IDC_MOUNTING;
	    err = "Mounting circle must be greater than 0";
	    GetDlgItemText(hWnd, ctrl, string, sizeof(string));
	    if ((temp = strtod(string, &end)) < 0.0 || *end != '\0')
		goto beep;
	    mounting = temp;

	    ctrl = IDC_CRANK;
	    GetDlgItemText(hWnd, ctrl, string, sizeof(string));
	    temp = strtod(string, &end);
	    if(*end != '\0')
		goto beep;
	    crank = temp;

	    ctrl = IDC_WEB;
	    err = "Web thickness must be greater than 0";
	    GetDlgItemText(hWnd, ctrl, string, sizeof(string));
	    if ((temp = strtod(string, &end)) < 0.0 || *end != '\0')
		goto beep;
	    web = temp;

	    /*
	     * More checks between related values.
	     */
	    ctrl = IDC_PCD;
	    err = "PCD too close to mounting circle";
	    if (pcd - bolt_hole < mounting)
		goto beep;

	    ctrl = IDC_TEETH;
	    err = "Number of teeth too small for PCD";
	    if ((n * chain[nchain].pitch) / PI - chain[nchain].roller < pcd + bolt_hole)
		goto beep;

	    ctrl = IDC_ECC;
	    err = "Sprocket too eccentric for PCD";
	    if (((n * chain[nchain].pitch) / PI) / sqrt(1 + ecc / 100.0) - chain[nchain].roller < pcd + bolt_hole)
		goto beep;

	    EndDialog(hWnd, LOWORD(wParam));
	    break;

	beep:
	    SetFocus(GetDlgItem(hWnd, ctrl));
	    SendDlgItemMessage(hWnd, ctrl, EM_SETSEL, 0, -1);
	    SetDlgItemText(hWnd, IDS_ERR, err);
	    MessageBeep(0);
	    break;

	case IDCANCEL:
	    nchain = saved_nchain;
	    n = saved_n;
	    ecc = saved_ecc;
	    pcd = saved_pcd;
	    bolt_hole = saved_bolt_hole;
            extra_rad = saved_extra;
	    nholes = saved_nholes;
	    mounting = saved_mounting;
	    crank = saved_crank;
	    web = saved_web;
            layout_lines = saved_layout;
	    EndDialog(hWnd, LOWORD(wParam));
	    break;

	case IDC_CHAIN:
	case IDC_TEETH:
	case IDC_ECC:
	case IDC_PCD:
	case IDC_BOLT_HOLE:
	case IDC_N_HOLES:
	case IDC_MOUNTING:
	case IDC_CRANK:
	case IDC_WEB:
	case IDC_LAYOUT:
	    if (HIWORD(wParam) == EN_SETFOCUS)
		focus = LOWORD(wParam);
	    break;

	default:
	    return 0;
	}

    default:
	return 0;
    }
    
    return 1;
}


/*
 * Main window procedure.
 */
int WINAPI
MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT	ps;
    HDC		hdc;
    PRINTDLG	pd;
    DOCINFO	di;
    int		rc;
    int		i, j;
    int		page, n_across, n_down;
    int		width, height;
    OPENFILENAME    ofn;
    char            dxf_file[128];
    char            file_title[64];

    switch (msg)
    {
    case WM_CREATE:
	break;

    case WM_PAINT:
	hdc = BeginPaint(hWnd, &ps);

	calc_initial();
	calc_sprocket();
	xoff = yoff = 0;
	plot(hdc);
		
	EndPaint(hWnd, &ps);

	break;

    case WM_COMMAND:
	switch (LOWORD(wParam))
	{
	case IDC_SETTINGS:
	    rc = DialogBox
		(
		    hInst, 
		    MAKEINTRESOURCE(IDD_SETTINGS), 
		    hWnd,
		    SettingsDlgProc
		);

	    if (rc != IDCANCEL)
		InvalidateRect(hWnd, NULL, TRUE);

	    if (rc == IDAPPLY)
		PostMessage(hWnd, WM_COMMAND, IDC_SETTINGS, 0);

	    break;

        case IDC_SAVEDXF:
            dxf_file[0] = '\0';
            memset(&ofn, 0, sizeof(OPENFILENAME));
	    ofn.lStructSize = sizeof(OPENFILENAME);
	    ofn.hwndOwner = hWnd;
	    ofn.lpstrFilter = "DXF files (*.dxf)\0*.dxf\0All Files (*.*)\0*.*\0";
	    ofn.lpstrTitle = "Save to DXF File";
	    ofn.lpstrFile = dxf_file;
	    ofn.nMaxFile = sizeof(dxf_file);
	    ofn.lpstrFileTitle = file_title;
	    ofn.nMaxFileTitle = sizeof(file_title);
	    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
	    ofn.lpstrDefExt = "dxf";

	    if (!GetSaveFileName(&ofn))
		break;

            dxf = dxf_open(dxf_file);
            save_dxf(dxf);
            dxf_close(dxf);

            break;

	case IDC_PRINT:
	    memset(&pd, 0, sizeof(pd));
	    pd.lStructSize = sizeof(pd);
	    pd.hwndOwner = hWnd;
	    pd.Flags = PD_RETURNDC | PD_ALLPAGES | PD_NOSELECTION;
	    pd.nFromPage = 0;
	    pd.nToPage = 0;
	    pd.nMinPage = 1;
	    pd.nMaxPage = 32767;

	    if (!PrintDlg(&pd))
		break;

	    memset(&di, 0, sizeof(di));
	    di.cbSize = sizeof(di);
	    di.lpszDocName = "Sprocket";
	    StartDoc(pd.hDC, &di);

	    calc_initial();
	    calc_sprocket();

	    /*
	     * Work out how many pages are required.
	     * Print them L to R within T to B.
	     */
	    width = GetDeviceCaps(pd.hDC, HORZSIZE) - OVERLAP;   /* Allow some overlap */
	    height = GetDeviceCaps(pd.hDC, VERTSIZE) - OVERLAP;
	    n_across = (int)ceil(2 * (a + MARG) / width);
	    n_down = (int)ceil(2 * (b + MARG) / height);
	    if (!(pd.Flags & PD_PAGENUMS))
	    {
		pd.nFromPage = 0;
		pd.nToPage = 32767;
	    }
	    page = 1;
	    for (i = 0, yoff = 0; i < n_down; i++, yoff += height)
	    {
		for (j = 0, xoff = 0; j < n_across; j++, xoff += width, page++)
		{
		    if (page < pd.nFromPage || page > pd.nToPage)
			continue;
		    
		    StartPage(pd.hDC);
		    
		    plot(pd.hDC);

		    /* 
		     * Registration marks.
		     */
		    if (j > 0)
		    {
			MoveToEx(pd.hDC, 5, 0, NULL);
			LineTo(pd.hDC, 5, -(height + OVERLAP) * 10);
		    }
		    if (j < n_across - 1)
		    {
			MoveToEx(pd.hDC, width * 10 + 5, 0, NULL);
			LineTo(pd.hDC, width * 10 + 5, -(height + OVERLAP) * 10);
		    }
		    if (i > 0)
		    {
			MoveToEx(pd.hDC, 0, -5, NULL);
			LineTo(pd.hDC, (width + OVERLAP) * 10, -5);
		    }
		    if (i < n_down - 1)
		    {
			MoveToEx(pd.hDC, 0, -(height * 10 + 5), NULL);
			LineTo(pd.hDC, (width + OVERLAP) * 10, -(height * 10 + 5));
		    }

		    EndPage(pd.hDC);
		}
	    }

	    EndDoc(pd.hDC);

	    break;

	case IDC_EXIT:
	    /*
	     * Exiting.
	     */
	    goto get_out;
	}
	break;

    case WM_DESTROY:
    get_out:
	if (tooth != NULL)
	    free(tooth);

	PostQuitMessage(0);

	break;

    default:
	return DefWindowProc(hWnd, msg, wParam, lParam);
    }

    return 1;
}


int WINAPI 
WinMain
(
    HINSTANCE hInstance,	// handle to current instance
    HINSTANCE hPrevInstance,	// handle to previous instance
    LPSTR lpCmdLine,	// pointer to command line
    int nCmdShow 	// show state of window
)
{
    MSG		msg; 
    WNDCLASS	wc; 
    HWND	hwndMain;
    
    /* Register the window class for the main window. */ 
 
    if (!hPrevInstance) 
    { 
        wc.style = 0; 
        wc.lpfnWndProc = (WNDPROC)MainWndProc; 
        wc.cbClsExtra = 0; 
        wc.cbWndExtra = 0; 
        wc.hInstance = hInstance; 
        wc.hIcon = LoadIcon((HINSTANCE) NULL, 
            MAKEINTRESOURCE(IDI_ICON1)); 
        wc.hCursor = LoadCursor((HINSTANCE) NULL, 
            IDC_ARROW); 
        wc.hbrBackground = GetStockObject(WHITE_BRUSH); 
        wc.lpszMenuName =  MAKEINTRESOURCE(IDR_MENU1); 
        wc.lpszClassName = "SprocketWndClass"; 
 
        if (!RegisterClass(&wc)) 
            return FALSE; 
    } 
 
    hInst = hInstance;  /* save instance handle */ 
 
    /* Create the main window. */ 
 
    hwndMain = CreateWindow("SprocketWndClass", "Sprocket", 
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 
        CW_USEDEFAULT, CW_USEDEFAULT, (HWND) NULL, 
        (HMENU) NULL, hInst, (LPVOID) NULL); 
 
    /* 
     * If the main window cannot be created, terminate 
     * the application. 
     */ 
 
    if (!hwndMain) 
        return FALSE; 
 
    /* Show the window and paint its contents. */ 
 
    ShowWindow(hwndMain, nCmdShow); 
    UpdateWindow(hwndMain); 

    /* Start the message loop. */ 
 
    while (GetMessage(&msg, (HWND) NULL, 0, 0)) 
    { 
        TranslateMessage(&msg); 
        DispatchMessage(&msg); 
    } 
 
    /* Return the exit code to Windows. */ 
 
    return msg.wParam; 
}
