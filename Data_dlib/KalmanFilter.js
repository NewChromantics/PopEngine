//	gr: from my old opencl kalman filter
//	https://gitlab.com/NewChromantics/PopLib/blob/master/data/TKalman.cl
//	which was originally from http://www.dzone.com/snippets/simple-kalman-filter-c
//	todo: document noise filter values. Generally use 0.001 to 1
//	todo: generic this, Everywhere with Vector3 can be swapped for another type
function KalmanFilter(InitialPos,QNoise,RNoise)
{
	this.lastcorrected = InitialPos;
	this.P_last = 0;
	this.QNoise = QNoise || 0.05;
	this.RNoise = RNoise || 0.50;
	this.TotalError = 0;
	this.x_est_last = InitialPos;

	this.Push = function(Position)
	{
		//do a prediction
		let x_temp_est = this.x_est_last;
		let P_temp = this.P_last + this.QNoise;

		//Debug("QNoise/" + QNoise + " - this.P_last/" + this.P_last );
		//Debug("x_temp_est/" + x_temp_est + " - P_temp/" + P_temp );

		//calculate the Kalman gain
		//	gr: timestep here?
		let KGain = P_temp * (1.0 / (P_temp + this.RNoise));
		
		//Debug("KGain/" + KGain + " - P_temp/" + P_temp );

		//	measured input
		let Accell = Position - this.lastcorrected;
		let z_measured = Accell;
		

		//	do correction to the estimate
		let x_est = x_temp_est + KGain * (z_measured - x_temp_est);
		//Debug("Accell/" + Accell + " - x_est/" + x_est );
		let P = (1 - KGain) * P_temp;
		
		//Debug("x_est/" + x_est + " - z_measured/" + z_measured );
		
		//	error is difference from the corrected estimate
		//let Error = Vector3.Distance(z_measured, x_est);
		let Error = x_est - z_measured;
		this.TotalError += Error;
		
		//	update our last's
		this.P_last = P;
		this.x_est_last = x_est;
		
		this.lastcorrected = this.lastcorrected + this.x_est_last;
		
		return Error;
	}
	
	this.GetAcceleration = function()
	{
		return this.x_est_last;
	}
	

	this.GetFilteredPosition = function()
	{
		return this.GetEstimatedPosition(0);
	}
	
	this.GetEstimatedPosition = function(TimeDelta)
	{
		let Accell = this.x_est_last;
		let NewPos = this.lastcorrected + (Accell * TimeDelta);
		return NewPos;
	}
	 
}

