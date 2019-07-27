# Spacely Sprockets

v1.1	Initial version, July 2002.
v2.0	Add Save to DXF option, at the request of Alaskabobb. Draw teeth in full for CNC/cutter machine input.
v2.1	Draw outline of all teeth as a single connected polyline, to make CNC cutting easier. 

sprocket.zip contains a pre built executable for Windows.

This program lets you generate a sprocket of any size, round or elliptical, with any PCD. I wrote this because I ran out of patterns from my old program of many years ago, and needed some more. Also, there has been a steady demand for them.
You can make nice chainrings for old non-standard crank spiders, or experiment with elliptical chainrings. Biopace chainrings are close to ellipses with a small eccentricity of about 10% (greater for smaller tooth counts)

There are only three menu items:

File/Settings... brings up a dialog box to allow you to change things:

- the pitch of chain (usually 0.5", expressed as a standard chain type)
- a clearance radius over the chain roller diameter
- number of teeth
- eccentricity, expressed as a percentage by which the major axis 
  exceeds the minor. Leave zero for a round sprocket.
- the bolt hole PCD (this is usually 110 or 130, but can be anything)
- the number of bolt holes (usually 5)
- the size of the bolt holes (8 and 10mm are common)
- the size of the mounting circle (usually 16mm less than the PCD 
  for 10mm bolts)
- the angle the crank makes with the major axis of the ellipse
- the amount of metal left between the scalloped cut-outs and the
  teeth. Make a large number to get rid of the scallops.
- an option to remove the layout lines and centre punch marks.

Default values are pre-loaded, and you are prevented from inputting silly values that send the numerical algorithms berserk. Everything is metric, but this shouldn't present a problem for the inchfooters, since crank spiders are all metric anyway.

File/Print... lets you print the pattern over one or more pages. If more than one page is printed, there is about 15mm of overlap and registration lines are printed in the area of overlap. This is to allow you to line up the patterns exactly, as all but the smallest sprockets will fall over onto at least two portrait A4 or Letter pages. If you have an A3 printer or a plotter, so much the better (I haven't tried it with a plotter, but it makes standard GDI line drawing calls, so should work)

File/Save to DXF... prompts for a filename and saves the sprocket pattern to a DXF file for loading into CAD programs or driving CNC machines.


Making the Sprocket
-------------------

Use spray adhesive to glue it onto a sheet of 2mm or 2.5mm (3/32") aluminium. The thicker size is better for strength but you will have to thin out the teeth more, unless the sprocket is for non-derailleur (1/8") chain. It's good to glue a sheet of blank paper on the other side of the metal to protect it from scratching. 

Center punch the holes for the teeth on the dots, and the mounting holes on the PCD of your crank. 

The holes should be drilled first with a small drill to ensure they stay on center. Then drill with an 8mm (5/16") drill. Cut through the outside of all the holes with a jigsaw leaving as much "meat" as possible on the teeth. Cut out the inside area in the pretty pattern of your choice, leaving enough metal for strength around the mounting holes. The scallops the program generates are a guide; the web that is left is variable in the program.

Then the slow part: filing off all the teeth! They need to be thinned somewhat too, especially if you are using 2.5mm sheet. Have a piece of NEW chain handy to check. It should roll on and off the teeth easily without sticking, in the absence of lubrication. With practice it goes quite fast, and you don't need to worry too much about taking too much metal off, within reason. It takes a couple of hours to make a 60T sprocket and it's good
exercise :-) 

If you put the sprocket onto the crank and turn it with the pedal you can thin the teeth with a file held in the other hand, and put points on them (the poor man's lathe). Other ideas, which I have found to work quite well, are to do a Hyperglide by thinning the teeth unevenly, or to remove every 4th or 5th tooth on all but the smallest sprocket. This helps shifting, especially if your tooth range is large. I have 44-56-68T on my trike with a
12-32 rear cluster. 

