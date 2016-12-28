#ifndef AFFINE_INVARIANT_FEATURES_AFFINE_INVARIANT_FEATURE
#define AFFINE_INVARIANT_FEATURES_AFFINE_INVARIANT_FEATURE

#include <affine_invariant_features/affine_invariant_feature_base.hpp>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/ref.hpp>

#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>

namespace affine_invariant_features {

//
// AffineInvariantFeature that samples features in various affine transformation space
//

class AffineInvariantFeature : public AffineInvariantFeatureBase {
private:
  // parallel loop helper
  struct ParallelLoopBody : public cv::ParallelLoopBody {
    virtual void operator()(const cv::Range &range) const {
      for (int i = range.start; i < range.end; ++i) {
        if (tasks[i]) {
          tasks[i]();
        }
      }
    }

    std::vector< boost::function< void() > > tasks;
  };

private:
  // the private constructor. users must use create() to instantiate an AffineInvariantFeature
  AffineInvariantFeature(const cv::Ptr< cv::Feature2D > base_feature)
      : AffineInvariantFeatureBase(base_feature) {}

public:
  virtual ~AffineInvariantFeature() {}

  // the only function to instantiate an AffineInvariantFeature
  static cv::Ptr< AffineInvariantFeature > create(const cv::Ptr< cv::Feature2D > base_feature) {
    return new AffineInvariantFeature(base_feature);
  }

  //
  // overloaded functions from AffineInvariantFeatureBase or its base class
  //

  // detect and compute affine invariant keypoints and descriptors
  virtual void detectAndCompute(cv::InputArray image, cv::InputArray mask,
                                std::vector< cv::KeyPoint > &keypoints, cv::OutputArray descriptors,
                                bool useProvidedKeypoints = false) {
    CV_Assert(base_feature_);

    // extract inputs
    const cv::Mat image_mat(image.getMat());
    const cv::Mat mask_mat(mask.getMat());

    // generate parameters for following parallel processing
    std::vector< double > tilt_params;
    std::vector< double > phi_params;
    tilt_params.push_back(1.);
    phi_params.push_back(0.);
    for (int i = 1; i < 6; ++i) {
      const double tilt(std::pow(2., 0.5 * i));
      for (double phi = 0.; phi < 180.; phi += 72. / tilt) {
        tilt_params.push_back(tilt);
        phi_params.push_back(phi);
      }
    }

    // prepare outputs of following parallel processing
    const int ntasks(tilt_params.size());
    std::vector< std::vector< cv::KeyPoint > > keypoints_array(ntasks);
    std::vector< cv::Mat > descriptors_array(ntasks);
    // TODO: make useProvidedKeypoints affect

    // bind each parallel task and arguments
    ParallelLoopBody body;
    for (int i = 0; i < ntasks; ++i) {
      body.tasks.push_back(boost::bind(
          &AffineInvariantFeature::detectAndComputeImpl, this, boost::ref(image_mat),
          boost::ref(mask_mat), boost::ref(keypoints_array[i]), boost::ref(descriptors_array[i]),
          tilt_params[i], phi_params[i], useProvidedKeypoints));
    }

    // do parallel tasks
    cv::parallel_for_(cv::Range(0, ntasks), body);

    // extend keypoints
    keypoints.clear();
    for (int i = 0; i < ntasks; ++i) {
      keypoints.insert(keypoints.end(), keypoints_array[i].begin(), keypoints_array[i].end());
    }

    // extend descriptors
    for (int i = 0; i < ntasks; ++i) {
      if (!descriptors_array[i].empty()) {
        descriptors.create(keypoints.size(), descriptors_array[i].cols,
                           descriptors_array[i].type());
        break;
      }
    }
    {
      cv::Mat descriptors_mat(descriptors.getMat());
      int rows(0);
      for (int i = 0; i < ntasks; ++i) {
        descriptors_array[i].copyTo(
            descriptors_mat.rowRange(rows, rows + descriptors_array[i].rows));
        rows += descriptors_array[i].rows;
      }
    }
  }

  virtual cv::String getDefaultName() const { return "AffineInvariantFeature"; }

private:
  void detectAndComputeImpl(const cv::Mat &src_image, const cv::Mat &src_mask,
                            std::vector< cv::KeyPoint > &keypoints, cv::Mat &descriptors,
                            const double tilt, const double phi, const bool useProvidedKeypoints) {
    CV_Assert(base_feature_);

    // affine transformation to be applied to the given image and mask
    cv::Matx32f affine(cv::Matx32f::eye());

    // apply the affine transformation to the image on the basis of the given parameters
    cv::Mat image(src_image.clone());
    if (phi != 0.) {
      // rotate the source frame
      affine = cv::getRotationMatrix2D(cv::Point2f(0., 0.), phi, 1.);
      cv::Rect tmp_rect;
      {
        std::vector< cv::Point2f > corners(4);
        corners[0] = cv::Point2f(0., 0.);
        corners[1] = cv::Point2f(image.cols, 0.);
        corners[2] = cv::Point2f(image.cols, image.rows);
        corners[3] = cv::Point2f(0., image.rows);
        std::vector< cv::Point2f > tmp_corners;
        cv::transform(corners, tmp_corners, affine);
        tmp_rect = cv::boundingRect(tmp_corners);
      }

      // cancel the offset of the rotated frame
      affine(0, 2) = -tmp_rect.x;
      affine(1, 2) = -tmp_rect.y;

      // apply the final transformation to the image
      cv::warpAffine(image, image, affine, tmp_rect.size(), cv::INTER_LINEAR, cv::BORDER_REPLICATE);
    }
    if (tilt != 1.) {
      // shrink the image in width
      cv::GaussianBlur(image, image, cv::Size(0, 0), 0.8 * std::sqrt(tilt * tilt - 1.), 0.01);
      cv::resize(image, image, cv::Size(0, 0), 1. / tilt, 1., cv::INTER_NEAREST);
      affine(0, 0) /= tilt;
      affine(0, 1) /= tilt;
      affine(0, 2) /= tilt;
    }

    // apply the affine transformation to the mask
    cv::Mat mask(src_mask.clone());
    if (mask.empty()) {
      mask = cv::Mat(src_image.size(), CV_8UC1, 255);
    }
    if (phi != 0. || tilt != 1.) {
      cv::warpAffine(mask, mask, affine, image.size(), cv::INTER_NEAREST);
    }

    // detect features in the skewed image
    base_feature_->detectAndCompute(image, mask, keypoints, descriptors, useProvidedKeypoints);

    // invert the positions of the detected keypoints
    cv::Matx32f invert_affine;
    cv::invertAffineTransform(affine, invert_affine);
    for (std::vector< cv::KeyPoint >::iterator keypoint = keypoints.begin();
         keypoint != keypoints.end(); ++keypoint) {
      cv::Vec2f pt(keypoint->pt.x, keypoint->pt.y);
      cv::transform(pt, pt, invert_affine);
      keypoint->pt.x = pt[0];
      keypoint->pt.y = pt[1];
    }
  }
};
}

#endif