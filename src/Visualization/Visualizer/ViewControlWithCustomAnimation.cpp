// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#include "ViewControlWithCustomAnimation.h"

#include <IO/ClassIO/IJsonConvertibleIO.h>

namespace three{

void ViewControlWithCustomAnimation::Reset()
{
	if (animation_mode_ == ANIMATION_FREEMODE) {
		ViewControl::Reset();
	}
}

void ViewControlWithCustomAnimation::ChangeFieldOfView(double step)
{
	if (animation_mode_ == ANIMATION_FREEMODE) {
		if (!view_trajectory_.view_status_.empty()) {
			// Once editing starts, lock ProjectionType.
			// This is because ProjectionType cannot be easily switched in a
			// smooth trajectory.
			if (GetProjectionType() == PROJECTION_PERSPECTIVE) {
				double old_fov = field_of_view_;
				ViewControl::ChangeFieldOfView(step);
				if (GetProjectionType() == PROJECTION_ORTHOGONAL) {
					field_of_view_ = old_fov;
				}
			} else {
				// do nothing, lock as PROJECTION_ORTHOGONAL
			}
			SetProjectionParameters();
		} else {
			ViewControl::ChangeFieldOfView(step);
		}
	}
}

void ViewControlWithCustomAnimation::Scale(double scale)
{
	if (animation_mode_ == ANIMATION_FREEMODE) {
		ViewControl::Scale(scale);
	}
}

void ViewControlWithCustomAnimation::Rotate(double x, double y, double xo, 
		double yo)
{
	if (animation_mode_ == ANIMATION_FREEMODE) {
		ViewControl::Rotate(x, y, xo, yo);
	}
}

void ViewControlWithCustomAnimation::Translate(double x, double y, double xo,
		double yo)
{
	if (animation_mode_ == ANIMATION_FREEMODE) {
		ViewControl::Translate(x, y, xo, yo);
	}
}

void ViewControlWithCustomAnimation::SetAnimationMode(AnimationMode mode)
{
	if (mode != ANIMATION_FREEMODE && view_trajectory_.view_status_.empty()) {
		return;
	}
	animation_mode_ = mode;
	switch (mode) {
	case ANIMATION_PREVIEWMODE:
	case ANIMATION_PLAYMODE:
		view_trajectory_.ComputeInterpolationCoefficients();
		GoToFirst();
		break;
	case ANIMATION_FREEMODE:
	default:
		break;
	}
}

void ViewControlWithCustomAnimation::AddKeyFrame()
{
	if (animation_mode_ == ANIMATION_FREEMODE) {
		ViewParameters current_status;
		ConvertToViewParameters(current_status);
		if (view_trajectory_.view_status_.empty()) {
			view_trajectory_.view_status_.push_back(current_status);
			current_keyframe_ = 0.0;
		} else {
			size_t current_index = CurrentKeyframe();
			view_trajectory_.view_status_.insert(
					view_trajectory_.view_status_.begin() + current_index + 1,
					current_status);
			current_keyframe_ = current_index + 1.0;
		}
	}
}

void ViewControlWithCustomAnimation::UpdateKeyFrame()
{
	if (animation_mode_ == ANIMATION_FREEMODE &&
			!view_trajectory_.view_status_.empty()) {
		ConvertToViewParameters(
				view_trajectory_.view_status_[CurrentKeyframe()]);
	}
}

void ViewControlWithCustomAnimation::DeleteKeyFrame()
{
	if (animation_mode_ == ANIMATION_FREEMODE &&
			!view_trajectory_.view_status_.empty()) {
		size_t current_index = CurrentKeyframe();
		view_trajectory_.view_status_.erase(
				view_trajectory_.view_status_.begin() + current_index);
		current_keyframe_ = RegularizeFrameIndex(current_index - 1.0,
				view_trajectory_.view_status_.size(),
				view_trajectory_.is_loop_);
	}
	SetViewControlFromTrajectory();
}

void ViewControlWithCustomAnimation::AddSpinKeyFrames(int num_of_key_frames
		/* = 20*/)
{
	if (animation_mode_ == ANIMATION_FREEMODE) {
		double radian_per_step = M_PI * 2.0 / double(num_of_key_frames);
		for (int i = 0; i < num_of_key_frames; i++) {
			ViewControl::Rotate(radian_per_step / ROTATION_RADIAN_PER_PIXEL, 0);
			AddKeyFrame();
		}
	}
}

std::string ViewControlWithCustomAnimation::GetStatusString()
{
	std::string prefix;
	switch (animation_mode_) {
	case ANIMATION_FREEMODE:
		prefix = "Editing ";
		break;
	case ANIMATION_PREVIEWMODE:
		prefix = "Previewing ";
		break;
	case ANIMATION_PLAYMODE:
		prefix = "Playing ";
		break;
	}
	char buffer[DEFAULT_IO_BUFFER_SIZE];
	if (animation_mode_ == ANIMATION_FREEMODE) {
		if (view_trajectory_.view_status_.empty()) {
			sprintf(buffer, "empty trajectory");
		} else {
			sprintf(buffer, "#%u keyframe (%u in total%s)",
					(unsigned int)CurrentKeyframe() + 1,
					(unsigned int)view_trajectory_.view_status_.size(),
					view_trajectory_.is_loop_ ? ", looped" : "");
		}
	} else {
		if (view_trajectory_.view_status_.empty()) {
			sprintf(buffer, "empty trajectory");
		} else {
			sprintf(buffer, "#%u frame (%u in total%s)",
					(unsigned int)CurrentFrame() + 1,
					(unsigned int)view_trajectory_.NumOfFrames(),
					view_trajectory_.is_loop_ ? ", looped" : "");
		}
	}
	return prefix + std::string(buffer);
}

void ViewControlWithCustomAnimation::Step(double change)
{
	if (view_trajectory_.view_status_.empty()) {
		return;
	}
	if (animation_mode_ == ANIMATION_FREEMODE) {
		current_keyframe_ += change;
		current_keyframe_ = RegularizeFrameIndex(current_keyframe_,
				view_trajectory_.view_status_.size(),
				view_trajectory_.is_loop_);
	} else {
		current_frame_ += change;
		current_frame_ = RegularizeFrameIndex(current_frame_,
				view_trajectory_.NumOfFrames(), view_trajectory_.is_loop_);
	}
	SetViewControlFromTrajectory();
}

void ViewControlWithCustomAnimation::GoToFirst()
{
	if (view_trajectory_.view_status_.empty()) {
		return;
	}
	if (animation_mode_ == ANIMATION_FREEMODE) {
		current_keyframe_ = 0.0;
	} else {
		current_frame_ = 0.0;
	}
	SetViewControlFromTrajectory();
}

void ViewControlWithCustomAnimation::GoToLast()
{
	if (view_trajectory_.view_status_.empty()) {
		return;
	}
	if (animation_mode_ == ANIMATION_FREEMODE) {
		current_keyframe_ = view_trajectory_.view_status_.size() - 1.0;
	} else {
		current_frame_ = view_trajectory_.NumOfFrames() - 1.0;
	}
	SetViewControlFromTrajectory();
}

bool ViewControlWithCustomAnimation::CaptureTrajectory(
		const std::string &filename/* = ""*/)
{
	if (view_trajectory_.view_status_.empty()) {
		return false;
	}
	std::string json_filename = filename;
	if (json_filename.empty()) {
		json_filename = "ViewTrajectory_" + GetCurrentTimeStamp() + ".json";
	}
	PrintDebug("[Visualizer] Trejactory capture to %s\n", json_filename.c_str());
	return WriteIJsonConvertible(json_filename, view_trajectory_);
}

bool ViewControlWithCustomAnimation::LoadTrajectoryFromJsonFile(
		const std::string &filename)
{
	bool success = ReadIJsonConvertible(filename, view_trajectory_);
	if (success == false) {
		view_trajectory_.Reset();
	}
	current_keyframe_ = 0.0;
	current_frame_ = 0.0;
	SetViewControlFromTrajectory();
	return success;
}

bool ViewControlWithCustomAnimation::LoadTrajectoryFromCameraTrajectory(
		const PinholeCameraTrajectory &camera_trajectory)
{
	current_keyframe_ = 0.0;
	current_frame_ = 0.0;
	view_trajectory_.Reset();
	if (camera_trajectory.extrinsic_.empty()) {
		return false;
	}
	view_trajectory_.interval_ = ViewTrajectory::INTERVAL_MIN;
	view_trajectory_.is_loop_ = false;
	view_trajectory_.view_status_.resize(camera_trajectory.extrinsic_.size());
	for (size_t i = 0; i < camera_trajectory.extrinsic_.size(); i++) {
		ViewControlWithCustomAnimation view_control = *this;
		if (view_control.ConvertFromPinholeCameraParameters(
				camera_trajectory.intrinsic_, 
				camera_trajectory.extrinsic_[i]) == false) {
			view_trajectory_.Reset();
			return false;
		}
		if (view_control.ConvertToViewParameters(
				view_trajectory_.view_status_[i]) == false) {
			view_trajectory_.Reset();
			return false;
		}
	}
	SetViewControlFromTrajectory();
	return true;
}

bool ViewControlWithCustomAnimation::IsValidPinholeCameraTrajectory()
{
	if (view_trajectory_.view_status_.empty()) {
		return false;
	}
	if (view_trajectory_.view_status_[0].field_of_view_ == FIELD_OF_VIEW_MIN) {
		return false;
	}
	for (const auto &status : view_trajectory_.view_status_) {
		if (status.field_of_view_ != 
				view_trajectory_.view_status_[0].field_of_view_) {
			return false;
		}
	}
	return true;
}

double ViewControlWithCustomAnimation::RegularizeFrameIndex(
		double current_frame, size_t num_of_frames, bool is_loop)
{
	if (num_of_frames == 0) {
		return 0.0;
	}
	double frame_index = current_frame;
	if (is_loop) {
		while (int(round(frame_index)) < 0) {
			frame_index += double(num_of_frames);
		}
		while (int(round(frame_index)) >= int(num_of_frames)) {
			frame_index -= double(num_of_frames);
		}
	} else {
		if (frame_index < 0.0) {
			frame_index = 0.0;
		}
		if (frame_index > num_of_frames - 1.0) {
			frame_index = num_of_frames - 1.0;
		}
	}
	return frame_index;
}

void ViewControlWithCustomAnimation::SetViewControlFromTrajectory()
{
	if (view_trajectory_.view_status_.empty()) {
		return;
	}
	if (animation_mode_ == ANIMATION_FREEMODE) {
		ConvertFromViewParameters(
				view_trajectory_.view_status_[CurrentKeyframe()]);
	} else {
		bool success;
		ViewParameters status;
		std::tie(success, status) = 
				view_trajectory_.GetInterpolatedFrame(CurrentFrame());
		if (success) {
			ConvertFromViewParameters(status);
		}
	}
}

}	// namespace three
