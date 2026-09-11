// Martin Duy Tat 28th July 2022

#include<omp.h>
#include<algorithm>
#include<vector>
#include<fstream>
#include<iostream>
#include<sstream>
#include<string>
#include<stdexcept>
#include"TCanvas.h"
#include"TF1.h"
#include"TLine.h"
#include"TRandom.h"
#include"TPad.h"
#include"TH1.h"
#include"ResolutionUtilities.h"
#include"RadiatorCell.h"
#include"ParticleTrack.h"
#include"Photon.h"
#include"PhotonMapper.h"
#include"PhotonReconstructor.h"
#include"SiPM.h"
#include"DifferentialEvolution.h"
#include"ResolutionOptimizable.h"
#include"Settings.h"
#include"RadiatorArray.h"
#include"Utilities.h"

namespace ResolutionUtilities {
  using Utilities::ResolutionStruct;

  namespace {
    /**
     * Suffixes of the parameter names written to and read from the fit result file
     */
    constexpr std::array<std::string_view, 5> ParameterNames{
      "Curvature",
      "XPosition",
      "ZPosition",
      "DetPosition",
      "DetTilt"};
    /**
     * Number of OpenMP threads used in the track loop when Optimisation/NumberThreads is not set
     */
    constexpr int DefaultNumberThreads = 8;
    int GetNumberThreads() {
      const int NumberThreads = Settings::Exists("Optimisation/NumberThreads")
	                      ? Settings::GetInt("Optimisation/NumberThreads")
	                      : DefaultNumberThreads;
      // num_threads(0) is undefined in OpenMP
      return std::max(1, NumberThreads);
    }
  }

  double CalculateResolution(const Tracks &Particles,
			     const RadiatorCell *radiatorCell,
			     const RadiatorArray &radiatorArray,
			     std::size_t Seed,
			     bool IncludeCentrePenalty) {
    // Trace the photons of each track in parallel, one result per track
    std::vector<ResolutionStruct> TrackResults(Particles.size());
    const int NumberThreads = GetNumberThreads();
    #pragma omp parallel for num_threads(NumberThreads)
    for(std::size_t i = 0; i < Particles.size(); i++) {
      // Each track gets its own photon sequence, independent of the thread schedule
      Utilities::Random().SetSeed(Utilities::TrackSeed(Seed, i));
      TrackResults[i] =
        Utilities::TrackPhotons(Particles[i], *radiatorCell, radiatorArray);
    }
    // Put it all together, in track order so that the sums do not depend on the thread schedule
    ResolutionStruct Total{};
    std::size_t PhotonsHitWallTracks = 0;
    for(const auto &resolutionStruct : TrackResults) {
      if(!resolutionStruct.HitCorrectCell) {
        continue;
      }
      if(resolutionStruct.HitTopWall) {
	PhotonsHitWallTracks++;
      } else if(resolutionStruct.N <= 1) {
        PhotonsHitWallTracks++;
      } else {
        Total.x += resolutionStruct.x;
        Total.N++;
        Total.CentreHitDistance += resolutionStruct.CentreHitDistance;
      }
    }
    if(Total.N == 0) {
      return 1000.0;
    }
    // 0.0003 is the pixel size angular resolution
    const double Resolution = Total.x/static_cast<double>(Total.N) + 0.0003*0.0003;
    const auto NumberParticles = Particles.size();
    auto GetFinalResolution = [&] (bool IncludeCentrePenalty) {
      // Penalty when hitting the upper wall
      const double WallPenalty = (10.0*static_cast<double>(PhotonsHitWallTracks))/
                                 static_cast<double>(NumberParticles);
      const double ResolutionWithPenalty = Resolution + WallPenalty;
      if(IncludeCentrePenalty) {
        // Penalty when far from the detector centre
        const Vector AverageRingPosition = Total.CentreHitDistance/Total.N;
        const double CentrePenalty = 0.01*TMath::Sqrt(AverageRingPosition.Mag2());
        return ResolutionWithPenalty + CentrePenalty;
      } else {
        return ResolutionWithPenalty;
      }
    };
    const double FinalResolution = GetFinalResolution(IncludeCentrePenalty);
    return FinalResolution;
  }

  double fcn(double MirrorCurvature,
	     double MirrorXPosition,
	     double MirrorZPosition,
	     double DetectorPosition,
	     double DetectorTilt,
	     RadiatorCell &radiatorCell,
	     const RadiatorArray &radiatorArray,
	     const Tracks &Particles,
	     std::size_t Seed,
	     bool IncludeCentrePenalty) {
    radiatorCell.SetMirrorCurvature(MirrorCurvature);
    radiatorCell.SetMirrorXPosition(MirrorXPosition);
    radiatorCell.SetMirrorZPosition(MirrorZPosition);
    radiatorCell.SetDetectorPosition(DetectorPosition);
    radiatorCell.SetDetectorTilt(DetectorTilt);
    if(!radiatorCell.IsDetectorInsideCell()) {
      return 1000.0;
    }
    const double Resolution = CalculateResolution(Particles,
	                                          &radiatorCell,
	                                          radiatorArray,
	                                          Seed,
	                                          IncludeCentrePenalty);
    return Resolution;
  }

  void PlotProjections(RadiatorCell &radiatorCell,
	               const RadiatorArray &radiatorArray,
                       const Tracks &Particles) {
    std::string ResultFilename = Settings::GetString("Optimisation/Filename");
    std::ifstream File(ResultFilename);
    if(!File.is_open()) {
      throw std::runtime_error("Cannot open fit result file " + ResultFilename);
    }
    std::vector<double> Result;
    std::string Line;
    while(std::getline(File, Line)) {
      std::string dummy;
      double Value;
      std::stringstream ss(Line);
      ss >> dummy >> Value;
      Result.push_back(Value);
    }
    File.close();
    const std::size_t NumberParameters = ParameterNames.size();
    if(Result.size() != NumberParameters) {
      throw std::runtime_error("Expected " + std::to_string(NumberParameters)
			       + " parameters in " + ResultFilename
			       + ", found " + std::to_string(Result.size()));
    }
    const std::size_t Seed = Settings::GetSizeT("General/Seed");
    auto MinimiseFunctionMirrorCurvature = [&] (double *x, double*) {
      return fcn(x[0], Result[1], Result[2], Result[3], Result[4],
	         radiatorCell, radiatorArray, Particles, Seed, false);
    };
    auto MinimiseFunctionXPosition = [&] (double *x, double*) {
      return fcn(Result[0], x[0], Result[2], Result[3], Result[4],
	         radiatorCell, radiatorArray, Particles, Seed, false);
    };
    auto MinimiseFunctionZPosition = [&] (double *x, double*) {
      return fcn(Result[0], Result[1], x[0], Result[3], Result[4],
	         radiatorCell, radiatorArray, Particles, Seed, false);
    };
    auto MinimiseFunctionDetPosition = [&] (double *x, double*) {
      return fcn(Result[0], Result[1], Result[2], x[0], Result[4],
	         radiatorCell, radiatorArray, Particles, Seed, false);
    };
    auto MinimiseFunctionDetTilt = [&] (double *x, double*) {
      return fcn(Result[0], Result[1], Result[2], Result[3], x[0],
	         radiatorCell, radiatorArray, Particles, Seed, false);
    };
    std::string Name1("Optimisation/MirrorCurvaturePlot_");
    double Curvature_min = Settings::GetDouble(Name1 + "min");
    double Curvature_max = Settings::GetDouble(Name1 + "max");
    TF1 f1("MirrorCurvature", MinimiseFunctionMirrorCurvature,
	   Curvature_min, Curvature_max, 0);
    std::string Name2("Optimisation/MirrorXPositionPlot_");
    double XPosition_min = Settings::GetDouble(Name2 + "min");
    double XPosition_max = Settings::GetDouble(Name2 + "max");
    TF1 f2("XPosition", MinimiseFunctionXPosition,
	   XPosition_min, XPosition_max, 0);
    std::string Name3("Optimisation/MirrorZPositionPlot_");
    double ZPosition_min = Settings::GetDouble(Name3 + "min");
    double ZPosition_max = Settings::GetDouble(Name3 + "max");
    TF1 f3("ZPosition", MinimiseFunctionZPosition,
	   ZPosition_min, ZPosition_max, 0);

    std::string Name4("Optimisation/DetectorPositionPlot_");
    double DetPosition_min = Settings::GetDouble(Name4 + "min");
    double DetPosition_max = Settings::GetDouble(Name4 + "max");
    TF1 f4("DetectorPosition", MinimiseFunctionDetPosition,
	   DetPosition_min, DetPosition_max, 0);
    std::string Name5("Optimisation/DetectorTiltPlot_");
    double DetTilt_min = Settings::GetDouble(Name5 + "min");
    double DetTilt_max = Settings::GetDouble(Name5 + "max");
    TF1 f5("DetectorTilt", MinimiseFunctionDetTilt,
	   DetTilt_min, DetTilt_max, 0);
    auto GetSolutionLine = [] (double Result, double ymax = 0.0) {
      TLine Solution(Result, 0.0, Result, 0.9*ymax);
      Solution.SetLineWidth(3);
      return Solution;
    };
    TCanvas c1("c1", "", 1200, 900);
    f1.SetTitle("Mirror curvature;Curvature (m);Resolution (rad)");
    f1.Draw();
    auto Solution1 = GetSolutionLine(Result[0], f1.GetHistogram()->GetMaximum());
    Solution1.Draw("SAME");
    c1.SaveAs("MirrorCurvatureOptimisation.pdf");
    TCanvas c2("c2", "", 1200, 900);
    f2.SetTitle("Horizontal mirror position;x (m);Resolution (rad)");
    f2.Draw();
    auto Solution2 = GetSolutionLine(Result[1], f2.GetHistogram()->GetMaximum());
    Solution2.Draw("SAME");
    c2.SaveAs("XPositionOptimisation.pdf");
    TCanvas c3("c3", "", 1200, 900);
    f3.SetTitle("Vertical mirror position;z (m);Resolution (rad)");
    f3.Draw();
    auto Solution3 = GetSolutionLine(Result[2], f3.GetHistogram()->GetMaximum());
    Solution3.Draw("SAME");
    c3.SaveAs("ZPositionOptimisation.pdf");
    TCanvas c4("c4", "", 1200, 900);
    f4.SetTitle("Horizontal detector position;x (m);Resolution (rad)");
    f4.Draw();
    auto Solution4 = GetSolutionLine(Result[3], f4.GetHistogram()->GetMaximum());
    Solution4.Draw("SAME");
    c4.SaveAs("DetectorPositionOptimisation.pdf");
    TCanvas c5("c5", "", 1200, 900);
    f5.SetTitle("Detector tilt angle;#theta (rad);Resolution (rad)");
    f5.Draw();
    auto Solution5 = GetSolutionLine(Result[4], f5.GetHistogram()->GetMaximum());
    Solution5.Draw("SAME");
    c5.SaveAs("DetectorTiltOptimisation.pdf");
  }

  void DoFit(RadiatorCell &radiatorCell,
	     const RadiatorArray &radiatorArray,
	     const Tracks &Particles,
	     std::size_t Column,
	     std::size_t Row) {
    ResolutionOptimizable resolutionOptimisable(radiatorCell, radiatorArray, Particles);
    const std::size_t NumberAgents = Settings::GetSizeT("Optimisation/NumberAgents");
    // Seed of the differential evolution search, separate from the photon seed
    const std::size_t DESeed = Settings::Exists("Optimisation/Seed")
	                     ? Settings::GetSizeT("Optimisation/Seed")
	                     : Settings::GetSizeT("General/Seed");
    de::DifferentialEvolution de(resolutionOptimisable, NumberAgents, DESeed);
    const int Iterations = Settings::GetInt("Optimisation/Iterations");
    de.Optimize(Iterations, true);
    auto Result = de.GetBestAgent();
    auto FixedParameters = resolutionOptimisable.GetFixedParameters();
    std::size_t TotalParameters = Result.size() + FixedParameters.size();
    std::vector<double> AllParameters;
    std::size_t j = 0;
    for(std::size_t i = 0; i < TotalParameters; i++) {
      auto iter = FixedParameters.find(i);
      if(iter != FixedParameters.end()) {
	AllParameters.push_back(iter->second);
      } else {
	AllParameters.push_back(Result[j]);
	j++;
      }
    }
    std::string ResultFilename = Settings::GetString("Optimisation/Filename");
    std::ofstream File(ResultFilename);
    const std::string BarrelOrEndcap = Settings::GetString("General/BarrelOrEndcap");
    const std::string Prefix = BarrelOrEndcap == "Barrel" ? "" : "EndCap";
    for(std::size_t i = 0; i < TotalParameters; i++) {
      File << Prefix;
      File << "Radiator_c" << Column << "_r" << Row << "_" << ParameterNames[i] << " ";
      File << AllParameters[i] << "\n";
    }
    //File << "\n" << "OptimalResolution: " << de.GetBestCost()*1000.0 << " mrad" << "\n";
    File.close();
  }

}
