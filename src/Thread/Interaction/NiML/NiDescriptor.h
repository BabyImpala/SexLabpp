#pragma once

#include <SimpleIni.h>

#include "NiType.h"
#include "Util/StringUtil.h"

namespace Thread::Interaction::NiML
{
    class INiDescriptor
    {
      public:
        virtual ~INiDescriptor() = default;

        virtual float Predict() const = 0;
        virtual NiType::Type GetType() const = 0;

      public:
        std::string GetCsvFeatureHeader() const
        {
            const auto numColumns = GetNumFeatures();
            std::string header = "";
            for (size_t i = 0; i < numColumns; i++) {
                header += std::format("f{}", i);
                if (i < numColumns - 1) {
                    header += ",";
                }
            }
            return header;
        }

        std::string GetCsvFeatureRow() const
        {
            const auto features = GetFeatures();
            assert(features.size() == GetNumFeatures());
            std::string row = "";
            for (size_t i = 0; i < features.size(); i++) {
                row += std::format("{}", features[i]);
                if (i < features.size() - 1) {
                    row += ",";
                }
            }
            return row;
        }

      protected:
        virtual std::vector<float> GetFeatures() const = 0;
        virtual size_t GetNumFeatures() const = 0;
    };

    template <NiType::Type Id = NiType::Type::None>
    class NiDescriptor :
      public INiDescriptor
    {
      public:
        NiDescriptor(std::vector<float> featureValues) : features(std::move(featureValues))
        {
            if (features.size() != GetNumFeatures()) {
                throw std::invalid_argument("Feature vector size does not match expected number of features");
            }
        }

        float Predict() const override { return std::inner_product(coefficients.begin(), coefficients.end(), features.begin(), bias); }
        NiType::Type GetType() const override { return Id; }

      public:
        static void Initialize(CSimpleIniA& inifile)
        {
            constexpr auto NaN = std::numeric_limits<float>::quiet_NaN();
            std::string section{ magic_enum::enum_name<NiType::Type>(Id) };
            bias = static_cast<float>(inifile.GetDoubleValue(section.c_str(), "bias", NaN));
            if (std::isnan(bias)) {
                const auto err = std::format("Descriptor '{}': Missing bias value", section);
                throw std::runtime_error(err);
            }
            const auto numFeatures = inifile.GetLongValue(section.c_str(), "num_features", 0);
            if (numFeatures <= 0) {
                const auto err = std::format("Descriptor '{}': Invalid number of features: {}", section, numFeatures);
                throw std::runtime_error(err);
            }
            const auto weightsStr = inifile.GetValue(section.c_str(), "weights", "");
            std::string_view weightsView{ weightsStr };
            const auto weightsVec = Util::StringSplit(weightsView, ";"sv);
            if (weightsVec.size() != static_cast<size_t>(numFeatures)) {
                const auto err = std::format("Descriptor '{}': Number of weights ({}) does not match num_features ({})", section, weightsVec.size(), numFeatures);
                throw std::runtime_error(err);
            }
            for (size_t i = 0; i < weightsVec.size(); i++) {
                coefficients[i] = std::stof(weightsVec[i].data());
            }
            logger::info("{}: Loaded [{}] ({}) coefficients, bias={:.3f}", section, coefficients, coefficients.size(), bias);
        }

      protected:
        std::vector<float> GetFeatures() const override { return features; }
        size_t GetNumFeatures() const override { return coefficients.size(); }

      private:
        std::vector<float> features{};
        static inline std::vector<float> coefficients{};
        static inline float bias{ 0.0f };
    };

}  // namespace Thread::Interaction::NiML
