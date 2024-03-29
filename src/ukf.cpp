#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

std::ofstream myfile_radar;
std::ofstream myfile_lidar;

/**
 * Initializes Unscented Kalman filter
 * This is scaffolding, do not modify
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  is_initialized_ = false;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 2;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = M_PI/2;
  
  //DO NOT MODIFY measurement noise values below these are provided by the sensor manufacturer.
  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  //DO NOT MODIFY measurement noise values above these are provided by the sensor manufacturer.
  //set state dimension
  n_x_ = 5;

  //define spreading parameter
  lambda_ = 3 - n_x_;
  
  n_aug_ = 7;
  
  n_z_radar = 3;
  n_z_lidar = 2;  
  /**
  TODO:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
  */
  // Augmented sigma points matrix
  X_aug_ = MatrixXd(n_aug_, (2*n_aug_+1));
  // predicted sigma points matrix
  Xsig_pred_ = MatrixXd(n_x_, 2 * n_aug_ + 1);
  //create vector for weights
  weights_ = VectorXd(2*n_aug_+1);
  
  z_radar_pred = VectorXd(n_z_radar); 
  
  z_radar = VectorXd(n_z_radar); 
  S_radar = MatrixXd(n_z_radar,n_z_radar);
  
  z_lidar = VectorXd(n_z_lidar); 
  S_lidar = MatrixXd(n_z_lidar,n_z_lidar);
  Zsig_radar = MatrixXd(n_z_radar, 2 * n_aug_ + 1);
  Zsig_lidar = MatrixXd(n_z_lidar, 2 * n_aug_ + 1);
  myfile_radar.open ("nis_radar.txt");

} 

UKF::~UKF() {
    myfile_radar.close();
    myfile_lidar.close();
}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
  if (!is_initialized_) {
      P_ = MatrixXd::Identity(5,5);
      if (meas_package.sensor_type_ == MeasurementPackage::RADAR) {
      /**
      Convert radar from polar to cartesian coordinates and initialize state.
      */
      float r = meas_package.raw_measurements_[0];
      float ang = meas_package.raw_measurements_[1];
      float vr = meas_package.raw_measurements_[2];
      
      float x,y;
      float vx,vy,v;
      float cos_ang = cos(ang);
      float sin_ang = sin(ang);
      x = r*cos_ang;
      y = r*sin_ang;
      v = 0;
      x_ << x, y, v,0,0;
      //cout<< r << ang << vr;
      cout<< "init state x" << x_<<endl;
      time_us_ = meas_package.timestamp_;
    }
    else if (meas_package.sensor_type_ == MeasurementPackage::LASER) {
      /**
      Initialize state.
      */
        x_ << meas_package.raw_measurements_[0], meas_package.raw_measurements_[1], 0, 0, 0;
        cout<< "init state x" << x_<<endl;

        time_us_ = meas_package.timestamp_;
    }

    // done initializing, no need to predict or update
  is_initialized_ = true;
  }
  
  float dt = (meas_package.timestamp_ - time_us_) / 1000000.0;	//dt - expressed in seconds
  time_us_ = meas_package.timestamp_;
  
  Prediction(dt);
  
  if((use_radar_ == true) && (meas_package.sensor_type_ == MeasurementPackage::RADAR))
  {
      UpdateRadar(meas_package);
  }
  else if((use_laser_ == true) && (meas_package.sensor_type_ == MeasurementPackage::LASER)){
      UpdateLidar(meas_package);
  }
}

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  TODO:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
  // first generate augmented sigma point
  AugmentedSigmaPoints(&X_aug_);
  // Second perdict sigma points
  SigmaPointPrediction(&Xsig_pred_,delta_t);
  // Predict the mean and covariance
  PredictMeanAndCovariance(&x_,&P_,&Xsig_pred_);
}

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  myfile_lidar.open ("nis_lidar.txt",ios::app);
  PredictLidarMeasurement(&z_lidar_pred, &S_lidar);
  z_lidar[0] = meas_package.raw_measurements_[0];
  z_lidar[1] = meas_package.raw_measurements_[1];
  //create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z_lidar);

/*******************************************************************************
 * Student part begin
 ******************************************************************************/

  //calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    //residual
    VectorXd z_diff = Zsig_lidar.col(i) - z_lidar_pred;
    
    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    
    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  //Kalman gain K;
  MatrixXd K = Tc * S_lidar.inverse();

  //residual
  VectorXd z_diff = z_lidar - z_lidar_pred;

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S_lidar*K.transpose();
  double nis = (z_diff.transpose() * S_lidar.inverse() * z_diff);
/*******************************************************************************
 * Student part end
 ******************************************************************************/

  //print result
  //std::cout << "Updated state lidar x: " << std::endl << x_ << std::endl;
  //std::cout << "Updated state covariance P: " << std::endl << P_ << std::endl;
  //std::cout << "The NIS for lidar is: " << nis << std::endl;
  myfile_lidar<<nis<<',';
  myfile_lidar .close();
}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  TODO:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
  myfile_radar.open ("nis_radar.txt",ios::app);
  PredictRadarMeasurement(&z_radar_pred, &S_radar);
  z_radar[0] = meas_package.raw_measurements_[0];
  z_radar[1] = meas_package.raw_measurements_[1];
  z_radar[2] = meas_package.raw_measurements_[2];
  
  //create matrix for cross correlation Tc
  MatrixXd Tc = MatrixXd(n_x_, n_z_radar);

/*******************************************************************************
 * Student part begin
 ******************************************************************************/

  //calculate cross correlation matrix
  Tc.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    //residual
    VectorXd z_diff = Zsig_radar.col(i) - z_radar_pred;
    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  //Kalman gain K;
  MatrixXd K = Tc * S_radar.inverse();

  //residual
  VectorXd z_diff = z_radar - z_radar_pred;

  //angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S_radar*K.transpose();

/*******************************************************************************
 * Student part end
 ******************************************************************************/

  double nis = (z_diff.transpose() * S_radar.inverse() * z_diff);
  
  //print result
  //std::cout << "Updated state radar x: " << std::endl << x_ << std::endl;
  //std::cout << "Updated state covariance P: " << std::endl << P_ << std::endl;
  //std::cout << "The NIS for radar is: " << nis << std::endl;
  myfile_radar<<nis<<',';
  myfile_radar .close();
}


void UKF::GenerateSigmaPoints(MatrixXd* Xsig_out) {
  lambda_ = 3 - n_x_;
  //create sigma point matrix
  MatrixXd Xsig = MatrixXd(n_x_, 2 * n_x_ + 1);

  //calculate square root of P
  MatrixXd A = P_.llt().matrixL();

/*******************************************************************************
 * Student part begin
 ******************************************************************************/

  //set first column of sigma point matrix
  Xsig.col(0)  = x_;

  //set remaining sigma points
  for (int i = 0; i < n_x_; i++)
  {
    Xsig.col(i+1)     = x_ + sqrt(lambda_+n_x_) * A.col(i);
    Xsig.col(i+1+n_x_) = x_ - sqrt(lambda_+n_x_) * A.col(i);
  }

/*******************************************************************************
 * Student part end
 ******************************************************************************/

  //print result
  //std::cout << "Xsig = " << std::endl << Xsig << std::endl;

  //write result
  *Xsig_out = Xsig;
}

void UKF::AugmentedSigmaPoints(MatrixXd* Xsig_out) {
  lambda_ = 3 - n_aug_;
  //create augmented mean vector
  VectorXd x_aug = VectorXd(7);

  //create augmented state covariance
  MatrixXd P_aug = MatrixXd(7, 7);

  //create sigma point matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_, 2 * n_aug_ + 1);

/*******************************************************************************
 * Student part begin
 ******************************************************************************/
 
  //create augmented mean state
  x_aug.head(5) = x_;
  x_aug(5) = 0;
  x_aug(6) = 0;

  //create augmented covariance matrix
  P_aug.fill(0.0);
  P_aug.topLeftCorner(5,5) = P_;
  P_aug(5,5) = std_a_*std_a_;
  P_aug(6,6) = std_yawdd_*std_yawdd_;

  //create square root matrix
  MatrixXd L = P_aug.llt().matrixL();

  //create augmented sigma points
  Xsig_aug.col(0)  = x_aug;
  for (int i = 0; i< n_aug_; i++)
  {
    Xsig_aug.col(i+1)       = x_aug + sqrt(lambda_+n_aug_) * L.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - sqrt(lambda_+n_aug_) * L.col(i);
  }
  
/*******************************************************************************
 * Student part end
 ******************************************************************************/

  //print result
  // std::cout << "Xsig_aug = " << std::endl << Xsig_aug << std::endl;

  //write result
  *Xsig_out = Xsig_aug;
}

void UKF::SigmaPointPrediction(MatrixXd* Xsig_out, double delta_t) {

  //create matrix with predicted sigma points as columns
  MatrixXd Xsig_pred = MatrixXd(n_x_, 2 * n_aug_ + 1);
/*******************************************************************************
 * Student part begin
 ******************************************************************************/

  //predict sigma points
  for (int i = 0; i< 2*n_aug_+1; i++)
  {
    //extract values for better readability
    double p_x = X_aug_(0,i);
    double p_y = X_aug_(1,i);
    double v = X_aug_(2,i);
    double yaw = X_aug_(3,i);
    double yawd = X_aug_(4,i);
    double nu_a = X_aug_(5,i);
    double nu_yawdd = X_aug_(6,i);

    //predicted state values
    double px_p, py_p;

    //avoid division by zero
    if (fabs(yawd) > 0.001) {
        px_p = p_x + v/yawd * ( sin (yaw + yawd*delta_t) - sin(yaw));
        py_p = p_y + v/yawd * ( cos(yaw) - cos(yaw+yawd*delta_t) );
    }
    else {
        px_p = p_x + v*delta_t*cos(yaw);
        py_p = p_y + v*delta_t*sin(yaw);
    }

    double v_p = v;
    double yaw_p = yaw + yawd*delta_t;
    double yawd_p = yawd;

    //add noise
    px_p = px_p + 0.5*nu_a*delta_t*delta_t * cos(yaw);
    py_p = py_p + 0.5*nu_a*delta_t*delta_t * sin(yaw);
    v_p = v_p + nu_a*delta_t;

    yaw_p = yaw_p + 0.5*nu_yawdd*delta_t*delta_t;
    yawd_p = yawd_p + nu_yawdd*delta_t;

    //write predicted sigma point into right column
    Xsig_pred(0,i) = px_p;
    Xsig_pred(1,i) = py_p;
    Xsig_pred(2,i) = v_p;
    Xsig_pred(3,i) = yaw_p;
    Xsig_pred(4,i) = yawd_p;
  }

/*******************************************************************************
 * Student part end
 ******************************************************************************/

  //print result
  // std::cout << "Xsig_pred = " << std::endl << Xsig_pred << std::endl;

  //write result
  *Xsig_out = Xsig_pred;

}


void UKF::PredictMeanAndCovariance(VectorXd* x_out, MatrixXd* P_out,MatrixXd * Xsig_pred) {
  //define spreading parameter
  lambda_ = 3 - n_aug_;
  
  //create vector for predicted state
  VectorXd x = VectorXd(n_x_);

  //create covariance matrix for prediction
  MatrixXd P = MatrixXd(n_x_, n_x_);


/*******************************************************************************
 * Student part begin
 ******************************************************************************/

  // set weights
  double weight_0 = lambda_/(lambda_+n_aug_);
  weights_(0) = weight_0;
  for (int i=1; i<2*n_aug_+1; i++) {  //2n+1 weights
    double weight = 0.5/(n_aug_+lambda_);
    weights_(i) = weight;
  }

  //predicted state mean
  x.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points
    x = x+ weights_(i) * Xsig_pred->col(i);
  }

  //predicted state covariance matrix
  P.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //iterate over sigma points

    // state difference
    VectorXd x_diff = Xsig_pred->col(i) - x;
    //angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    P = P + weights_(i) * x_diff * x_diff.transpose() ;
  }


/*******************************************************************************
 * Student part end
 ******************************************************************************/

  //print result
  //std::cout << "Predicted state" << std::endl;
  //std::cout << x << std::endl;
  //std::cout << "Predicted covariance matrix" << std::endl;
  //std::cout << P << std::endl;
  //write result
  *x_out = x;
  *P_out = P;
}



void UKF::PredictRadarMeasurement(VectorXd* z_out, MatrixXd* S_out) {
  //set measurement dimension, radar can measure r, phi, and r_dot
  int n_z = 3;

  //set vector for weights
  double weight_0 = lambda_/(lambda_+n_aug_);
  weights_(0) = weight_0;
  for (int i=1; i<2*n_aug_+1; i++) {  
    double weight = 0.5/(n_aug_+lambda_);
    weights_(i) = weight;
  }
  
  //create matrix for sigma points in measurement space
  // MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

/*******************************************************************************
 * Student part begin
 ******************************************************************************/
  
  //transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    // extract values for better readibility
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v  = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);

    double v1 = cos(yaw)*v;
    double v2 = sin(yaw)*v;

    // measurement model
    Zsig_radar(0,i) = sqrt(p_x*p_x + p_y*p_y);                        //r
    Zsig_radar(1,i) = atan2(p_y,p_x);                                 //phi
    Zsig_radar(2,i) = (p_x*v1 + p_y*v2 ) / sqrt(p_x*p_x + p_y*p_y);   //r_dot
  }
  //std::cout << "z_pred: " << std::endl << Zsig_radar << std::endl;
  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {
      z_pred = z_pred + weights_(i) * Zsig_radar.col(i);
  }

  //innovation covariance matrix S
  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig_radar.col(i) - z_pred;

    //angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  //add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z,n_z);
  R <<    std_radr_*std_radr_, 0, 0,
          0, std_radphi_*std_radphi_, 0,
          0, 0,std_radrd_*std_radrd_;
  S = S + R;

  
/*******************************************************************************
 * Student part end
 ******************************************************************************/

  //print result
  //std::cout << "z_pred: " << std::endl << z_pred << std::endl;
  //std::cout << "S: " << std::endl << S << std::endl;

  //write result
  *z_out = z_pred;
  *S_out = S;
}


void UKF::PredictLidarMeasurement(VectorXd* z_out, MatrixXd* S_out) {
  //set measurement dimension, radar can measure r, phi, and r_dot
  int n_z = 2;

  //set vector for weights
   double weight_0 = lambda_/(lambda_+n_aug_);
  weights_(0) = weight_0;
  for (int i=1; i<2*n_aug_+1; i++) {  
    double weight = 0.5/(n_aug_+lambda_);
    weights_(i) = weight;
  }
  
  //create matrix for sigma points in measurement space
  // MatrixXd Zsig = MatrixXd(n_z, 2 * n_aug_ + 1);

/*******************************************************************************
 * Student part begin
 ******************************************************************************/

  //transform sigma points into measurement space
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points

    // extract values for better readibility
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v  = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);

    // measurement model
    Zsig_lidar(0,i) = p_x;                        //px
    Zsig_lidar(1,i) = p_y;                                 //py
  }

  //mean predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);
  for (int i=0; i < 2*n_aug_+1; i++) {
      z_pred = z_pred + weights_(i) * Zsig_lidar.col(i);
  }

  //innovation covariance matrix S
  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);
  for (int i = 0; i < 2 * n_aug_ + 1; i++) {  //2n+1 simga points
    //residual
    VectorXd z_diff = Zsig_lidar.col(i) - z_pred;
    S = S + weights_(i) * z_diff * z_diff.transpose();
  }

  //add measurement noise covariance matrix
  MatrixXd R = MatrixXd(n_z,n_z);
  R <<    std_laspx_*std_laspx_, 0,
          0, std_laspy_*std_laspy_;
  S = S + R;

  
/*******************************************************************************
 * Student part end
 ******************************************************************************/

  //print result
  // std::cout << "z_pred: " << std::endl << z_pred << std::endl;
  // std::cout << "S: " << std::endl << S << std::endl;

  //write result
  *z_out = z_pred;
  *S_out = S;
}