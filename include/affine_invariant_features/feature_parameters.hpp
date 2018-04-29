#ifndef AFFINE_INVARIANT_FEATURES_FEATURE_PARAMETERS
#define AFFINE_INVARIANT_FEATURES_FEATURE_PARAMETERS

#include <string>

#include <affine_invariant_features/affine_invariant_feature.hpp>
#include <affine_invariant_features/cv_serializable.hpp>

#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/xfeatures2d.hpp>

namespace affine_invariant_features {

//
// A base class for parameter sets of feature detectors and descriptor extractors
//

struct FeatureParameters : public CvSerializable {
public:
  FeatureParameters() {}

  virtual ~FeatureParameters() {}

  virtual cv::Ptr< cv::Feature2D > createFeature() const = 0;
};

//
// Affine invariant sampled feature
//

static cv::Ptr< FeatureParameters > createFeatureParameters(const std::string &);

struct AIFParameters : public std::vector< cv::Ptr< FeatureParameters > >,
                       public FeatureParameters {
public:
  AIFParameters() {}

  virtual ~AIFParameters() {}

  virtual cv::Ptr< cv::Feature2D > createFeature() const {
    switch (size()) {
    case 0:
      return cv::Ptr< cv::Feature2D >();
    case 1:
      return AffineInvariantFeature::create((*this)[0] ? (*this)[0]->createFeature()
                                                       : cv::Ptr< cv::Feature2D >());
    default:
      return AffineInvariantFeature::create(
          (*this)[0] ? (*this)[0]->createFeature() : cv::Ptr< cv::Feature2D >(),
          (*this)[1] ? (*this)[1]->createFeature() : cv::Ptr< cv::Feature2D >());
    }
  }

  virtual void read(const cv::FileNode &fn) {
    clear();
    for (cv::FileNodeIterator node = fn.begin(); node != fn.end(); ++node) {
      if (!(*node).isNamed()) { // operator-> did not work
        continue;
      }
      const cv::Ptr< FeatureParameters > p(createFeatureParameters((*node).name()));
      if (!p) {
        continue;
      }
      p->read(*node);
      push_back(p);
    }
  }

  virtual void write(cv::FileStorage &fs) const {
    for (std::vector< cv::Ptr< FeatureParameters > >::const_iterator p = begin(); p != end(); ++p) {
      if (!(*p)) {
        continue;
      }
      fs << (*p)->getDefaultName() << "{";
      (*p)->write(fs);
      fs << "}";
    }
  }

  virtual std::string getDefaultName() const { return "AIFParameters"; }
};

//
// AKAZE
//

struct AKAZEParameters : public FeatureParameters {
public:
  AKAZEParameters()
      : descriptorType(defaultAKAZE().getDescriptorType()),
        descriptorSize(defaultAKAZE().getDescriptorSize()),
        descriptorChannels(defaultAKAZE().getDescriptorChannels()),
        threshold(defaultAKAZE().getThreshold()), nOctaves(defaultAKAZE().getNOctaves()),
        nOctaveLayers(defaultAKAZE().getNOctaveLayers()),
        diffusivity(defaultAKAZE().getDiffusivity()) {}

  virtual ~AKAZEParameters() {}

  virtual cv::Ptr< cv::Feature2D > createFeature() const {
    return cv::AKAZE::create(descriptorType, descriptorSize, descriptorChannels, threshold,
                             nOctaves, nOctaveLayers, diffusivity);
  }

  virtual void read(const cv::FileNode &fn) {
    fn["descriptorType"] >> descriptorType;
    fn["descriptorSize"] >> descriptorSize;
    fn["descriptorChannels"] >> descriptorChannels;
    fn["threshold"] >> threshold;
    fn["nOctaves"] >> nOctaves;
    fn["nOctaveLayers"] >> nOctaveLayers;
    fn["diffusivity"] >> diffusivity;
  }

  virtual void write(cv::FileStorage &fs) const {
    fs << "descriptorType" << descriptorType;
    fs << "descriptorSize" << descriptorSize;
    fs << "descriptorChannels" << descriptorChannels;
    fs << "threshold" << threshold;
    fs << "nOctaves" << nOctaves;
    fs << "nOctaveLayers" << nOctaveLayers;
    fs << "diffusivity" << diffusivity;
  }

  virtual std::string getDefaultName() const { return "AKAZEParameters"; }

protected:
  static const cv::AKAZE &defaultAKAZE() {
    static cv::Ptr< const cv::AKAZE > default_akaze(cv::AKAZE::create());
    CV_Assert(default_akaze);
    return *default_akaze;
  }

public:
  int descriptorType;
  int descriptorSize;
  int descriptorChannels;
  float threshold;
  int nOctaves;
  int nOctaveLayers;
  int diffusivity;
};

//
// BRISK
//

struct BRISKParameters : public FeatureParameters {
public:
  // Note: like SIFT, no interface to access default BRISK parameters
  BRISKParameters() : threshold(30), nOctaves(3), patternScale(1.0f) {}

  virtual ~BRISKParameters() {}

  virtual cv::Ptr< cv::Feature2D > createFeature() const {
    return cv::BRISK::create(threshold, nOctaves, patternScale);
  }

  virtual void read(const cv::FileNode &fn) {
    fn["threshold"] >> threshold;
    fn["nOctaves"] >> nOctaves;
    fn["patternScale"] >> patternScale;
  }

  virtual void write(cv::FileStorage &fs) const {
    fs << "threshold" << threshold;
    fs << "nOctaves" << nOctaves;
    fs << "patternScale" << patternScale;
  }

  virtual std::string getDefaultName() const { return "BRISKParameters"; }

public:
  int threshold;
  int nOctaves;
  float patternScale;
};

//
// SIFT
//

struct SIFTParameters : public FeatureParameters {
public:
  // opencv does not provide interfaces to access default SIFT parameters.
  // thus values below are copied from online reference.
  SIFTParameters()
      : nfeatures(0), nOctaveLayers(3), contrastThreshold(0.04), edgeThreshold(10.), sigma(1.6) {}

  virtual ~SIFTParameters() {}

  virtual cv::Ptr< cv::Feature2D > createFeature() const {
    return cv::xfeatures2d::SIFT::create(nfeatures, nOctaveLayers, contrastThreshold, edgeThreshold,
                                         sigma);
  }

  virtual void read(const cv::FileNode &fn) {
    fn["nfeatures"] >> nfeatures;
    fn["nOctaveLayers"] >> nOctaveLayers;
    fn["contrastThreshold"] >> contrastThreshold;
    fn["edgeThreshold"] >> edgeThreshold;
    fn["sigma"] >> sigma;
  }

  virtual void write(cv::FileStorage &fs) const {
    fs << "nfeatures" << nfeatures;
    fs << "nOctaveLayers" << nOctaveLayers;
    fs << "contrastThreshold" << contrastThreshold;
    fs << "edgeThreshold" << edgeThreshold;
    fs << "sigma" << sigma;
  }

  virtual std::string getDefaultName() const { return "SIFTParameters"; }

public:
  int nfeatures;
  int nOctaveLayers;
  double contrastThreshold;
  double edgeThreshold;
  double sigma;
};

//
// SURF
//

struct SURFParameters : public FeatureParameters {
public:
  SURFParameters()
      : hessianThreshold(defaultSURF().getHessianThreshold()),
        nOctaves(defaultSURF().getNOctaves()), nOctaveLayers(defaultSURF().getNOctaveLayers()),
        extended(defaultSURF().getExtended()), upright(defaultSURF().getUpright()) {}

  virtual ~SURFParameters() {}

  virtual cv::Ptr< cv::Feature2D > createFeature() const {
    return cv::xfeatures2d::SURF::create(hessianThreshold, nOctaves, nOctaveLayers, extended,
                                         upright);
  }

  virtual void read(const cv::FileNode &fn) {
    fn["hessianThreshold"] >> hessianThreshold;
    fn["nOctaves"] >> nOctaves;
    fn["nOctaveLayers"] >> nOctaveLayers;
    fn["extended"] >> extended;
    fn["upright"] >> upright;
  }

  virtual void write(cv::FileStorage &fs) const {
    fs << "hessianThreshold" << hessianThreshold;
    fs << "nOctaves" << nOctaves;
    fs << "nOctaveLayers" << nOctaveLayers;
    fs << "extended" << extended;
    fs << "upright" << upright;
  }

  virtual std::string getDefaultName() const { return "SURFParameters"; }

protected:
  static const cv::xfeatures2d::SURF &defaultSURF() {
    static cv::Ptr< const cv::xfeatures2d::SURF > default_surf(cv::xfeatures2d::SURF::create());
    CV_Assert(default_surf);
    return *default_surf;
  }

public:
  double hessianThreshold;
  int nOctaves;
  int nOctaveLayers;
  bool extended;
  bool upright;
};

//
// Utility functions to create or read variants of FeatureParameters
//

#define AIF_RETURN_IF_CREATE(type)                                                                 \
  do {                                                                                             \
    const cv::Ptr< type > params(new type());                                                      \
    if (type_name == params->getDefaultName()) {                                                   \
      return params;                                                                               \
    }                                                                                              \
  } while (false)

static inline cv::Ptr< FeatureParameters > createFeatureParameters(const std::string &type_name) {
  AIF_RETURN_IF_CREATE(AIFParameters);
  AIF_RETURN_IF_CREATE(AKAZEParameters);
  AIF_RETURN_IF_CREATE(BRISKParameters);
  AIF_RETURN_IF_CREATE(SIFTParameters);
  AIF_RETURN_IF_CREATE(SURFParameters);
  return cv::Ptr< FeatureParameters >();
}

#define AIF_RETURN_IF_LOAD(type)                                                                   \
  do {                                                                                             \
    const cv::Ptr< type > params(load< type >(fn));                                                \
    if (params) {                                                                                  \
      return params;                                                                               \
    }                                                                                              \
  } while (false)

template <> cv::Ptr< FeatureParameters > load< FeatureParameters >(const cv::FileNode &fn) {
  AIF_RETURN_IF_LOAD(AIFParameters);
  AIF_RETURN_IF_LOAD(AKAZEParameters);
  AIF_RETURN_IF_LOAD(BRISKParameters);
  AIF_RETURN_IF_LOAD(SIFTParameters);
  AIF_RETURN_IF_LOAD(SURFParameters);
  return cv::Ptr< FeatureParameters >();
}

} // namespace affine_invariant_features

#endif