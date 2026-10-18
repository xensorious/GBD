#ifndef COASTLINE_VEHICLE_PROJECTION_H
#define COASTLINE_VEHICLE_PROJECTION_H

/* The road and car sprites use the same chase camera. Keep the visible car
 * rectangles here so gameplay contact never drifts away from the artwork. */
#define DRIVE_SCREEN_WIDTH 256
#define DRIVE_SCREEN_HEIGHT 192
#define DRIVE_HORIZON 20
#define DRIVE_CAMERA 190
#define DRIVE_PLAYER_Y 160
#define DRIVE_NEAR_Z (-44)
#define DRIVE_PLAYER_ART_W 48
#define DRIVE_PLAYER_ART_H 72
#define DRIVE_TRAFFIC_ART_W 40
#define DRIVE_TRAFFIC_ART_H 64

typedef struct { int left,top,width,height; } VehicleScreenRect;

static inline int vehicle_project_y(int z)
{return DRIVE_HORIZON+((DRIVE_PLAYER_Y-DRIVE_HORIZON)*DRIVE_CAMERA)/(z+DRIVE_CAMERA);}

static inline int vehicle_project_scale(int z)
{return (276*DRIVE_CAMERA)/(z+DRIVE_CAMERA);}

static inline int vehicle_project_x(int world_x,int camera_center,int camera_x,int z)
{return DRIVE_SCREEN_WIDTH/2+((world_x-camera_center-camera_x)*vehicle_project_scale(z))/256;}

static inline int vehicle_yaw_pose(int yaw)
{return yaw<-70?0:yaw<-12?1:yaw>70?4:yaw>12?3:2;}

static inline VehicleScreenRect vehicle_player_rect(int center_x,int bike,int lift)
{
 int w=bike?24:29,h=bike?41:42;
 VehicleScreenRect r={center_x-w/2,DRIVE_PLAYER_Y-h/2-lift,w,h};
 return r;
}

static inline VehicleScreenRect vehicle_traffic_rect(int center_x,int z)
{
 int size=(vehicle_project_scale(z)*180)/256;
 int w=DRIVE_TRAFFIC_ART_W*size/256,h=DRIVE_TRAFFIC_ART_H*size/256;
 VehicleScreenRect r={center_x-w/2,vehicle_project_y(z)-h/2,w,h};
 return r;
}

#endif
