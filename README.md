A nano arduino reads 16 temperature sensing diodes using a multiplexor that allows the arduino to read each diode individually. When it has finished reading all 16 diodes, the arduino determines which diode had the highest temperature and then sends this information via i2c. If any of the diode's temperatures are greater than 60 degrees Celsius or if there is something wrong with the sensor itself (i.e., it does not work), the arduino will cause a red led to blink. The hardware directory includes both the schematics and altium projects.

In addition, the documentation directory includes a flow chart, design documents and a code plan.

Lastly, the software directory contains the c programming language used for this project.
