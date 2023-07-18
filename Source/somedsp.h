#pragma once

#include <vector>

// note that limits can't be the same and the loop may run for long!
template<typename T>
inline T reflect_value(const T& minval, const T& val, const T& maxval)
{
	T temp = val;
	while (temp<minval || temp>maxval)
	{
		if (temp < minval)
			temp = minval + (minval - temp);
		if (temp > maxval)
			temp = maxval + (maxval - temp);
	}
	return temp;
}

// note that limits can't be the same and the loop may run for long!
template<typename T>
inline T wrap_value(const T& minval, const T& val, const T& maxval)
{
	T temp = val;
	while (temp<minval || temp>maxval)
	{
		if (temp < minval)
			temp = maxval - (minval - temp);
		if (temp > maxval)
			temp = minval - (maxval - temp);
	}
	return temp;
}

// Stuff taken from VCV Rack SDK...

inline float rescale(float x, float xMin, float xMax, float yMin, float yMax) {
	return yMin + (x - xMin) / (xMax - xMin) * (yMax - yMin);
}

template <typename T>
T amplitudeToDb(T amp) {
	return std::log10(amp) * 20;
}

template <typename T>
T dbToAmplitude(T db) {
	return std::pow(10, db / 20);
}

/** Digital IIR filter processor.
https://en.wikipedia.org/wiki/Infinite_impulse_response
*/
template <int B_ORDER, int A_ORDER, typename T = float>
struct IIRFilter {
	/** transfer function numerator coefficients: b_0, b_1, etc.
	*/
	T b[B_ORDER] = {};
	/** transfer function denominator coefficients: a_1, a_2, etc.
	a_0 is fixed to 1 and omitted from the `a` array, so its indices are shifted down by 1.
	*/
	T a[A_ORDER - 1] = {};
	/** input state
	x[0] = x_{n-1}
	x[1] = x_{n-2}
	etc.
	*/
	T x[B_ORDER - 1];
	/** output state */
	T y[A_ORDER - 1];

	IIRFilter() {
		reset();
	}

	void reset() {
		for (int i = 1; i < B_ORDER; i++) {
			x[i - 1] = 0.f;
		}
		for (int i = 1; i < A_ORDER; i++) {
			y[i - 1] = 0.f;
		}
	}

	void setCoefficients(const T* b, const T* a) {
		for (int i = 0; i < B_ORDER; i++) {
			this->b[i] = b[i];
		}
		for (int i = 1; i < A_ORDER; i++) {
			this->a[i - 1] = a[i - 1];
		}
	}

	T process(T in) {
		T out = 0.f;
		// Add x state
		if (0 < B_ORDER) {
			out = b[0] * in;
		}
		for (int i = 1; i < B_ORDER; i++) {
			out += b[i] * x[i - 1];
		}
		// Subtract y state
		for (int i = 1; i < A_ORDER; i++) {
			out -= a[i - 1] * y[i - 1];
		}
		// Shift x state
		for (int i = B_ORDER - 1; i >= 2; i--) {
			x[i - 1] = x[i - 2];
		}
		x[0] = in;
		// Shift y state
		for (int i = A_ORDER - 1; i >= 2; i--) {
			y[i - 1] = y[i - 2];
		}
		y[0] = out;
		return out;
	}

	/** Computes the complex transfer function $H(s)$ at a particular frequency
	s: normalized angular frequency equal to $2 \pi f / f_{sr}$ ($\pi$ is the Nyquist frequency)
	*/
	std::complex<T> getTransferFunction(T s) {
		// Compute sum(a_k z^-k) / sum(b_k z^-k) where z = e^(i s)
		std::complex<T> bSum(b[0], 0);
		std::complex<T> aSum(1, 0);
		for (int i = 1; i < std::max(B_ORDER, A_ORDER); i++) {
			T p = -i * s;
			std::complex<T> z(std::cos(p), std::sin(p));
			if (i < B_ORDER)
				bSum += b[i] * z;
			if (i < A_ORDER)
				aSum += a[i - 1] * z;
		}
		return bSum / aSum;
	}

	T getFrequencyResponse(T f) {
		// T hReal, hImag;
		// getTransferFunction(2 * M_PI * f, &hReal, &hImag);
		// return simd::hypot(hReal, hImag);
		return std::abs(getTransferFunction(2 * M_PI * f));
	}

	T getFrequencyPhase(T f) {
		return std::arg(getTransferFunction(2 * M_PI * f));
	}
};

template <typename T = float>
struct TBiquadFilter : IIRFilter<3, 3, T> {
	enum Type {
		LOWPASS_1POLE,
		HIGHPASS_1POLE,
		LOWPASS,
		HIGHPASS,
		LOWSHELF,
		HIGHSHELF,
		BANDPASS,
		PEAK,
		NOTCH,
		NUM_TYPES
	};

	TBiquadFilter() {
		setParameters(LOWPASS, 0.f, 0.f, 1.f);
	}

	/** Calculates and sets the biquad transfer function coefficients.
	f: normalized frequency (cutoff frequency / sample rate), must be less than 0.5
	Q: quality factor
	V: gain
	*/
	void setParameters(Type type, float f, float Q, float V) {
		float K = std::tan(M_PI * f);
		switch (type) {
			case LOWPASS_1POLE: {
				this->a[0] = -std::exp(-2.f * M_PI * f);
				this->a[1] = 0.f;
				this->b[0] = 1.f + this->a[0];
				this->b[1] = 0.f;
				this->b[2] = 0.f;
			} break;

			case HIGHPASS_1POLE: {
				this->a[0] = std::exp(-2.f * M_PI * (0.5f - f));
				this->a[1] = 0.f;
				this->b[0] = 1.f - this->a[0];
				this->b[1] = 0.f;
				this->b[2] = 0.f;
			} break;

			case LOWPASS: {
				float norm = 1.f / (1.f + K / Q + K * K);
				this->b[0] = K * K * norm;
				this->b[1] = 2.f * this->b[0];
				this->b[2] = this->b[0];
				this->a[0] = 2.f * (K * K - 1.f) * norm;
				this->a[1] = (1.f - K / Q + K * K) * norm;
			} break;

			case HIGHPASS: {
				float norm = 1.f / (1.f + K / Q + K * K);
				this->b[0] = norm;
				this->b[1] = -2.f * this->b[0];
				this->b[2] = this->b[0];
				this->a[0] = 2.f * (K * K - 1.f) * norm;
				this->a[1] = (1.f - K / Q + K * K) * norm;

			} break;

			case LOWSHELF: {
				float sqrtV = std::sqrt(V);
				if (V >= 1.f) {
					float norm = 1.f / (1.f + M_SQRT2 * K + K * K);
					this->b[0] = (1.f + M_SQRT2 * sqrtV * K + V * K * K) * norm;
					this->b[1] = 2.f * (V * K * K - 1.f) * norm;
					this->b[2] = (1.f - M_SQRT2 * sqrtV * K + V * K * K) * norm;
					this->a[0] = 2.f * (K * K - 1.f) * norm;
					this->a[1] = (1.f - M_SQRT2 * K + K * K) * norm;
				}
				else {
					float norm = 1.f / (1.f + M_SQRT2 / sqrtV * K + K * K / V);
					this->b[0] = (1.f + M_SQRT2 * K + K * K) * norm;
					this->b[1] = 2.f * (K * K - 1) * norm;
					this->b[2] = (1.f - M_SQRT2 * K + K * K) * norm;
					this->a[0] = 2.f * (K * K / V - 1.f) * norm;
					this->a[1] = (1.f - M_SQRT2 / sqrtV * K + K * K / V) * norm;
				}
			} break;

			case HIGHSHELF: {
				float sqrtV = std::sqrt(V);
				if (V >= 1.f) {
					float norm = 1.f / (1.f + M_SQRT2 * K + K * K);
					this->b[0] = (V + M_SQRT2 * sqrtV * K + K * K) * norm;
					this->b[1] = 2.f * (K * K - V) * norm;
					this->b[2] = (V - M_SQRT2 * sqrtV * K + K * K) * norm;
					this->a[0] = 2.f * (K * K - 1.f) * norm;
					this->a[1] = (1.f - M_SQRT2 * K + K * K) * norm;
				}
				else {
					float norm = 1.f / (1.f / V + M_SQRT2 / sqrtV * K + K * K);
					this->b[0] = (1.f + M_SQRT2 * K + K * K) * norm;
					this->b[1] = 2.f * (K * K - 1.f) * norm;
					this->b[2] = (1.f - M_SQRT2 * K + K * K) * norm;
					this->a[0] = 2.f * (K * K - 1.f / V) * norm;
					this->a[1] = (1.f / V - M_SQRT2 / sqrtV * K + K * K) * norm;
				}
			} break;

			case BANDPASS: {
				float norm = 1.f / (1.f + K / Q + K * K);
				this->b[0] = K / Q * norm;
				this->b[1] = 0.f;
				this->b[2] = -this->b[0];
				this->a[0] = 2.f * (K * K - 1.f) * norm;
				this->a[1] = (1.f - K / Q + K * K) * norm;
			} break;

			case PEAK: {
				if (V >= 1.f) {
					float norm = 1.f / (1.f + K / Q + K * K);
					this->b[0] = (1.f + K / Q * V + K * K) * norm;
					this->b[1] = 2.f * (K * K - 1.f) * norm;
					this->b[2] = (1.f - K / Q * V + K * K) * norm;
					this->a[0] = this->b[1];
					this->a[1] = (1.f - K / Q + K * K) * norm;
				}
				else {
					float norm = 1.f / (1.f + K / Q / V + K * K);
					this->b[0] = (1.f + K / Q + K * K) * norm;
					this->b[1] = 2.f * (K * K - 1.f) * norm;
					this->b[2] = (1.f - K / Q + K * K) * norm;
					this->a[0] = this->b[1];
					this->a[1] = (1.f - K / Q / V + K * K) * norm;
				}
			} break;

			case NOTCH: {
				float norm = 1.f / (1.f + K / Q + K * K);
				this->b[0] = (1.f + K * K) * norm;
				this->b[1] = 2.f * (K * K - 1.f) * norm;
				this->b[2] = this->b[0];
				this->a[0] = this->b[1];
				this->a[1] = (1.f - K / Q + K * K) * norm;
			} break;

			default: break;
		}
	}
};

typedef TBiquadFilter<> BiquadFilter;

// End VCV Rack stuff
