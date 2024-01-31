$fn = 50;

module mount(x,y,size = 2.4, support = false) { // For 3mm screw
    translate([x,y,0])
    difference(){
    cylinder($mount_height,r=size);
    cylinder($mount_height + 0.01,r=(size/2));
    }
}

mount(83.218883, 115.787348);
mount(210.921045, 128.347347);
mount(171.254582, 128.347343);
mount(94.65393, 200.237347);
mount(242.938889, 204.527349);
mount(139.236742, 204.527345);
mount(287.5217, 200.237345);
mount(298.956749, 115.787345);