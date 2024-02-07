$fn = 50;
$extrude_height = 3;
$mount_height=$extrude_height + 7;

module mount(x,y,size = 2.7, support = false) { // For 3mm screw
    translate([x,y,0])
    difference(){
    cylinder($mount_height,r=size+0.5);
    cylinder($mount_height + 0.01,r=(size/2));
    }
}

rotate([0, 0, 180]){
//mount(-19.833233, -33.877503125);
mount(96.433885, 38.012498875);
//mount(-96.433885, 38.012500875);
mount(19.83323, -33.877499125);
mount(51.851074, 42.302502875);
//mount(-51.851073, 42.302498875);
mount(107.868934, -46.437501125);
//mount(-107.868932, -46.437498125); 
}

difference() {

union(){
// base
rotate([0, 0, 0])
translate([-241.452/2, -103.149/2, 0])
linear_extrude(height=3)
import("/home/beef/src/niubkb/case/base_left.dxf");

// hole support
rotate([0, 0, 0])
translate([-241.452/2, -103.149/2, 0])
linear_extrude(height=$mount_height)
import("/home/beef/src/niubkb/case/hole_support_left.dxf");
}

// add small space between halves
translate([-1, -60, 0])
cube([2, 100, 20]);
}