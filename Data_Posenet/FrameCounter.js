

function PopFrameRateCounter()
{
	this.LastFrameRateTimelapse = Date.now();
	this.FrameCounters = [];
	this.FrameCounterLastFps = [];
	this.UpdateFrequency = 1000;		//	doesnt have to be every second!
	
	this.CheckFrameRateLapse = function()
	{
		let Now = Date.now();
		if ( Now - this.LastFrameRateTimelapse < this.UpdateFrequency )
			return;
		//	1 sec has lapsed
		//	ideally timelapsed = 1
		let TimeLapsed = (Now - this.LastFrameRateTimelapse) / 1000;
		
		let This = this;
		let UpdateCounter = function(CounterName)
		{
			let Count = This.FrameCounters[CounterName];
			This.FrameCounters[CounterName] = 0;
			let Fps = Count / TimeLapsed;
			This.FrameCounterLastFps[CounterName] = Fps;
			Debug( CounterName + " " + Fps.toFixed(2) + "fps");
		}
		let CounterNames = Object.keys(this.FrameCounters);
		CounterNames.forEach( UpdateCounter );
		this.LastFrameRateTimelapse = Now;
	}
	
	this.UpdateFrameCounter = function(CounterName)
	{
		if ( this.FrameCounters[CounterName] === undefined )
			this.FrameCounters[CounterName] = 0;
		this.FrameCounters[CounterName]++;
		this.CheckFrameRateLapse();
	}
	
	this.GetFrameCounter = function(CounterName)
	{
		if ( this.FrameCounterLastFps[CounterName] === undefined )
			return 0;
		return this.FrameCounterLastFps[CounterName];
	}
}


var _FrameRateCounter = new PopFrameRateCounter();
function UpdateFrameCounter(CounterName)
{
	_FrameRateCounter.UpdateFrameCounter(CounterName);
}

function GetFrameCounter(CounterName)
{
	return _FrameRateCounter.GetFrameCounter(CounterName);
}

