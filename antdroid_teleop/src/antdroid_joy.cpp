/* antdroid_joy.cpp: provides control of antdroid robot using keyboard or joy.
 *
 * Copyright (C) 2015 Alexander Gil and Javier Román
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <ros/ros.h>
#include <sensor_msgs/Joy.h>
#include "boost/thread/mutex.hpp"
#include "boost/thread/thread.hpp"
#include "ros/console.h"
#include "../include/antdroid_teleop/antdroid_joy.hpp"

AntdroidTeleop::AntdroidTeleop():
    _ph("~"),

    _rotate_right           (MI_BOTON_R1),
    _rotate_left            (MI_BOTON_L1),

    _change_speed           (MI_CRUCETA_ARR_ABA),
    _change_height          (MI_CRUCETA_IZ_DER),
    _change_foot            (MI_CRUCETA_ARR_ABA),
    _change_step            (MI_CRUCETA_IZ_DER),

    _walk_y                 (MI_STICK_IZQUIERDO_IZ_DER),
    _walk_x                 (MI_STICK_IZQUIERDO_ARR_ABA),
    _change_gait            (MI_BOTON_L3),

    _balance_x              (MI_STICK_DERECHO_ARR_ABA),
    _balance_y              (MI_STICK_DERECHO_IZ_DER),
    _balance_z              (MI_STICK_DERECHO_IZ_DER),
    _balance_default        (MI_BOTON_R3),

    _attack                 (MI_BOTON_TRIANGULO),
    _engage                 (MI_BOTON_START),
    _disengage              (MI_BOTON_SELECT),

    _action_button          (MI_BOTON_R2),

    _last_pitch             (0),
    _last_roll              (0),
    _last_yaw               (0),

    _gait_type              (1),

    _new_balance_msg        (false),
    _new_balance_z_msg      (false),
    _new_gait_msg           (false),
    _new_height_msg         (false),
    _new_foot_msg           (false),
    _new_rotate_msg         (false),
    _new_speed_msg          (false),
    _new_vel_msg            (false),
    _new_step_msg           (false),
    _new_balance_default_msg(false),
    _new_attack_msg         (false),
    _new_engage_msg         (false),
    _new_disengage_msg      (false)
{      
 
    sub_joy = _nh.subscribe<sensor_msgs::Joy>("joy", 1, &AntdroidTeleop::joyCallback, this);
    
    _pub_vel         = _ph.advertise<geometry_msgs::Twist>("cmd_vel", 1);
    _pub_balance     = _ph.advertise<antdroid_msgs::Balance>("balance", 1);
    _pub_gait        = _ph.advertise<antdroid_msgs::Gait>("gait", 1);
    _pub_height      = _ph.advertise<antdroid_msgs::Height>("height", 1);
    _pub_foot        = _ph.advertise<antdroid_msgs::Foot>("foot", 1);
    _pub_speed       = _ph.advertise<antdroid_msgs::Speed>("speed", 1);
    _pub_step        = _ph.advertise<std_msgs::Bool>("step", 1);

    _timer = _nh.createTimer(ros::Duration(0.1), boost::bind(&AntdroidTeleop::publish, this));
}

void AntdroidTeleop::joyCallback(const sensor_msgs::Joy::ConstPtr& joy)
{ 
    /*****************  SPEED ********* CROSS PAD *****************************/    
    if(!joy->buttons[_action_button] &&
      (joy->axes[_change_speed] > DEAD_ZONE || joy->axes[_change_speed] < - DEAD_ZONE))
    {
        if(joy->axes[_change_speed] >= DEAD_ZONE)
            _msg_changeSpeed.speed = RISE_SPEED;
        if(joy->axes[_change_speed] <= - DEAD_ZONE)
            _msg_changeSpeed.speed = DECREASE_SPEED;

        _new_speed_msg = true;
    }

    /*****************  HEIGHT ********* CROSS PAD *****************************/ 
    if(!joy->buttons[_action_button] &&
      (joy->axes[_change_height] > DEAD_ZONE || joy->axes[_change_height] < - DEAD_ZONE))
    {
        if(joy->axes[_change_height] >= DEAD_ZONE)
            _msg_height.height = DECREASE_HEIGHT;
        if(joy->axes[_change_height] <= - DEAD_ZONE)
            _msg_height.height = RISE_HEIGHT;
        
        _new_height_msg = true;
    }

    /*****************  FOOT ********* CROSS PAD + R2 ************************/ 
    if(joy->buttons[_action_button] && 
      (joy->axes[_change_foot] > DEAD_ZONE || joy->axes[_change_foot] < - DEAD_ZONE))
    {
        if(joy->axes[_change_foot] >= DEAD_ZONE)
            _msg_foot.footDistance = RISE_FOOT;
        if(joy->axes[_change_foot] <= - DEAD_ZONE)
            _msg_foot.footDistance = DECREASE_FOOT;

        _new_foot_msg = true;
    }

    /*****************  STEP ********* CROSS PAD + R2 ************************/ 
    if(joy->buttons[_action_button] && 
      (joy->axes[_change_step] > DEAD_ZONE || joy->axes[_change_step] < - DEAD_ZONE))
    {
        if(joy->axes[_change_step] >= DEAD_ZONE)
            _msg_step.data = false;
        if(joy->axes[_change_step] <= - DEAD_ZONE)
            _msg_step.data = true;

        _new_step_msg = true;
    }

    /*****************  GAIT **************************************************/
    if(joy->buttons[_change_gait])
    {
        if(_gait_type == TRIPOD_MODE)
            _gait_type = RIPPLE_MODE;  //type [1, 2] -> [tripod, riple]
        else
            _gait_type = TRIPOD_MODE;

        _msg_gait.type = _gait_type;

        _new_gait_msg = true;
    }

    /*****************  WALK *************************************************/
    if(joy->axes[_walk_x] || joy->axes[_walk_y])
    {
        _msg_vel.linear.x = joy->axes[_walk_x];
            if(_msg_vel.linear.x > DEAD_ZONE)         _msg_vel.linear.x = 1;
            else if(_msg_vel.linear.x < -DEAD_ZONE)   _msg_vel.linear.x = -1;
            else                                      _msg_vel.linear.x = 0;

        _msg_vel.linear.y = joy->axes[_walk_y];
            if(_msg_vel.linear.y > DEAD_ZONE)         _msg_vel.linear.y = 1;
            else if(_msg_vel.linear.y < -DEAD_ZONE)   _msg_vel.linear.y = -1;
            else                                      _msg_vel.linear.y = 0;

        _msg_vel.angular.z = 0;

        /*ROS_INFO("_msg_vel(linear.x, linear.y, angular.z) =  ( %f, %f, %f) ",
            _msg_vel.linear.x,_msg_vel.linear.y,_msg_vel.angular.z);*/

        _new_vel_msg = true;
    }
    
    /*****************  ROTATE ************************************************/
    if(joy->buttons[_rotate_left] && !joy->buttons[_rotate_right])
    {
        _msg_vel.linear.x = 0;
        _msg_vel.linear.y = 0;
        _msg_vel.angular.z = ROTATE_LEFT;

        _new_rotate_msg = true;
    }

    if(joy->buttons[_rotate_right] && !joy->buttons[_rotate_left])
    {
        _msg_vel.linear.x = 0;
        _msg_vel.linear.y = 0;
        _msg_vel.angular.z = ROTATE_RIGHT;

        _new_rotate_msg = true;
    }

    /*****************  BALANCE ***********************************************/
    if(!joy->buttons[_action_button] &&
      (joy->axes[_balance_x] || joy->axes[_balance_y]))
    {
        _pitch = joy->axes[_balance_x];
            if(_pitch > DEAD_ZONE)         _pitch = 1;
            else if(_pitch < - DEAD_ZONE)  _pitch = -1;
            else                           _pitch = 0;
        
        _roll = joy->axes[_balance_y];
            if(_roll > DEAD_ZONE)         _roll = 1;
            else if(_roll < -DEAD_ZONE)   _roll = -1;
            else                          _roll = 0;

        _yaw = 0;

        _new_balance_msg = true;
    }

    if(joy->buttons[_action_button] && (joy->axes[_balance_z]))
    {
        _msg_balance.pitch = 0;
        _msg_balance.roll = 0 ;

        _last_pitch = 0;
        _last_roll = 0;

        _yaw = joy->axes[_balance_z];
            if(_yaw > DEAD_ZONE)        _yaw = 1;
            else if(_yaw < - DEAD_ZONE) _yaw = -1;
            else                        _yaw = 0;

        _new_balance_z_msg = true;
    }

    if(joy->buttons[_balance_default])
    {
        _msg_balance.pitch = 0;
        _msg_balance.roll = 0;
        _msg_balance.yaw = 0;

        _last_pitch = 0;
        _last_roll = 0;
        _last_yaw = 0;

        _new_balance_default_msg = true;
    }

    /*****************  ATTACK ************************************************/
    /*if(joy->buttons[_attack])
    {

        _new_attack_msg = true;
    }

    /*****************  ENGAGE ************************************************
    if(joy->buttons[_engage])
    {

        _new_engage_msg = true;
    }

    /*****************  DISENGAGE *********************************************
    if(joy->buttons[_disengage])
    {

        _new_disengage_msg = true;
    }*/
}

void AntdroidTeleop::publish()
{
    boost::mutex::scoped_lock lock(_publish_mutex);

    if(_new_gait_msg)
    {
        ROS_INFO_STREAM("publish:: gait");
        _pub_gait.publish(_msg_gait);
        _new_gait_msg = false;
    }

    if(_new_height_msg)
    {
        ROS_INFO_STREAM("publish:: height");
        _pub_height.publish(_msg_height);
        _new_height_msg = false;
    }

    if(_new_foot_msg)
    {
        ROS_INFO_STREAM("publish:: foot");
        _pub_foot.publish(_msg_foot);
        _new_foot_msg = false;
    }

    if(_new_speed_msg)
    {
        ROS_INFO_STREAM("publish:: speed");
        _pub_speed.publish(_msg_changeSpeed);
        _new_speed_msg = false;
    }

    if(_new_step_msg)
    {
        ROS_INFO_STREAM("publish:: step");
        _pub_step.publish(_msg_step);
        _new_step_msg = false;
    }

    if(_new_vel_msg)
    {
        ROS_INFO_STREAM("publish:: walk");
        _pub_vel.publish(_msg_vel);
        _new_vel_msg = false;
    }

    if(_new_rotate_msg)
    {
        ROS_INFO_STREAM("publish:: rotate");
        _pub_vel.publish(_msg_vel);
        _new_rotate_msg = false;
    }

    if(_new_balance_msg)
    {
        ROS_INFO_STREAM("publish:: balance");
        manageBalance();
        _pub_balance.publish(_msg_balance);
        _new_balance_msg = false;
    }

    if(_new_balance_z_msg)
    {
        ROS_INFO_STREAM("publish:: balance z");
        manageBalance();
        /*ROS_INFO("msg_balance [pitch, roll, yaw]: [ %d, %d, %d]", 
            _msg_balance.pitch, _msg_balance.roll,_msg_balance.yaw);*/
        _pub_balance.publish(_msg_balance);
        _new_balance_z_msg = false;
    }

    if(_new_balance_default_msg)
    {
        ROS_INFO_STREAM("publish:: balance default");
        /*ROS_INFO("msg_balance [pitch, roll, yaw]: [ %d, %d, %d]", 
            _msg_balance.pitch, _msg_balance.roll,_msg_balance.yaw);*/

        _pub_balance.publish(_msg_balance);
        _new_balance_default_msg = false;
    }
/*
    if(_new_attack_msg)
    {
        ROS_INFO_STREAM("publish:: attack");
        _new_attack_msg = false;
    }

    if(_new_engage_msg)
    {
        ROS_INFO_STREAM("publish:: engage");
        _new_engage_msg = false;
    }

    if(_new_disengage_msg)
    {
        ROS_INFO_STREAM("publish:: disengage");
        _new_disengage_msg = false;
    }*/
}

void AntdroidTeleop::manageBalance()
{
    _last_pitch = updateAngle(_pitch, _last_pitch);
    _last_roll = updateAngle(_roll, _last_roll);
    _last_yaw = updateAngle(_yaw, _last_yaw);

    _msg_balance.pitch = _last_pitch;
    _msg_balance.roll = _last_roll;
    _msg_balance.yaw = _last_yaw;
}

int AntdroidTeleop::updateAngle(int axis, int last_angle)
{
    _max_angle_step = ANGLE_STEP * 5;

    if ((last_angle + axis * ANGLE_STEP > (- _max_angle_step)) & 
        (last_angle + axis * ANGLE_STEP < _max_angle_step) & (axis != 0))
    {
        last_angle += axis * ANGLE_STEP;
    }

    return(last_angle);
}

int main(int argc, char** argv)
{
    ros::init(argc, argv, "antdroid_teleop");
    AntdroidTeleop antdroid_teleop;

    ros::spin();
}