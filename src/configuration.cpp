#include <iostream>

#include <configuration.hpp>
#include <layer.hpp>


ConfigurationManager::ConfigurationManager(const std::string& filename):
    jsonParser(filename)
    {}

void ConfigurationManager::configure() {
    // Parse Config file
    jsonParser.parse();
    auto jsonTree = jsonParser.getJsonTree();
    jsonTree->print();

   // Mode
    auto modeObject = jsonTree->find("mode");
    SolverMode mode;
    if (modeObject.has_value()) {
        auto modeIt = modeObject.value();
        if (std::holds_alternative<std::string>(modeIt->second->value)) {
            std::string modeLabel = std::get<std::string>(modeIt->second->value);
            if (modeLabel=="Simulation"){
                mode = SolverMode::Simulation;
            }
            else if (modeLabel=="Fitting") {
                mode = SolverMode::Fitting;
            }
            else {throw std::runtime_error("Invalid mode.");}
        }
        else {throw std::runtime_error("Invalid mode. It should be a string.");}
    }
    else {throw std::runtime_error("You need to provide a mode!");}
    input.mode = mode;
   

    // Materials
    auto stack = jsonTree->find("stack");
    std::vector<Material> materials;
    std::vector<double> thicknesses;
    double emitterPosition;
    bool hasEmitter = false;
    Eigen::Index emitterIndex;
    if (stack.has_value()) {
        auto it = stack.value();
        if (std::holds_alternative<Json::JsonList*>(it->second->value)) {
            auto layerList = std::get<Json::JsonList*>(it->second->value);
            for (auto layerIt = layerList->begin(); layerIt != layerList->end(); ++layerIt) {
                if (std::holds_alternative<Json::JsonObject*>((*layerIt)->value)) {
                    auto layer = std::get<Json::JsonObject*>((*layerIt)->value);
                    auto layerObject = layer->begin();
                    auto layerData = std::get<Json::JsonObject*>(layerObject->second->value);
                    auto materialIt = layerData->find("material");
                    if (materialIt != layerData->end()) {
                        if (std::holds_alternative<double>(materialIt->second->value)) {
                            materials.emplace_back(std::get<double>(materialIt->second->value), 0.0);
                            if (layerObject->first == "Substrate") {
                                materials.emplace_back(std::get<double>(materialIt->second->value), 0.0);
                            }
                        }
                        else if (std::holds_alternative<std::string>(materialIt->second->value)) {
                            materials.emplace_back(std::get<std::string>(materialIt->second->value), ',');
                        }
                        else {throw std::runtime_error("Invalid material entry!");}
                    }
                    else {throw std::runtime_error("Each layer must have a material!");}
                    if (layerObject->first != "Environment" && layerObject->first != "Substrate") {
                        auto thicknessIt = layerData->find("thickness");
                        if (thicknessIt != layerData->end()) {
                            if (std::holds_alternative<double>(thicknessIt->second->value)) {
                                thicknesses.push_back(std::get<double>(thicknessIt->second->value)*(1e-9));
                            }
                            else {throw std::runtime_error("Thickness must be a numeric value!");}
                        }
                        else {throw std::runtime_error("Every internal layer must have a thickness!");}
                        if (layerObject->first == "Emitter") {
                            hasEmitter = true;
                            emitterIndex = layerIt - layerList->begin();
                            auto emitterIt = layerData->find("position"); 
                            if (emitterIt != layerData->end()) {
                               if (std::holds_alternative<double>(emitterIt->second->value)) {
                                emitterPosition = std::get<double>(emitterIt->second->value);
                                input.hasDipoleDistribution = false;
                            }
                                else if (std::holds_alternative<std::string>(emitterIt->second->value)) {
                                    input.hasDipoleDistribution = true;
                                    std::string dipoleDistributionLabel = std::get<std::string>(emitterIt->second->value);
                                    if (dipoleDistributionLabel != "uniform") {throw std::runtime_error("Invali dipole distribution type!");}
                                    double zmin = thicknesses.size() > 1 ? *(thicknesses.end()-2) : 0;
                                    double zmax = thicknesses.back();
                                    std::cout << "Zmin: " << zmin << ", Zmax: " << zmax << std::endl;
                                    input.dipoleDist = DipoleDistribution(zmin, zmax, DipoleDistributionType::Uniform);
                                }
                                else {throw std::runtime_error("Emitter position must be a numeric value!");} 
                            }
                        }
                    }
                }
            }
        }
    }
    else {throw std::runtime_error("You need to provide a stack!");}
    if (!hasEmitter) {throw std::runtime_error("You need at least one emitter!");}
    thicknesses.push_back(5000e-10);

    std::vector<Layer> layers;
    for (size_t i = 0; i < materials.size(); ++i) {
        if (i == 0) {
            layers.emplace_back(materials[i], -1.0);
        }
        else {
            layers.emplace_back(materials[i], thicknesses[i-1]);
        }
    }
    layers[emitterIndex].isEmitter = true;
    input.layers = std::move(layers);
    input.emitterPosition = emitterPosition*(1e-9);

    // Wavelength and Spectrum
    auto wvlObject = jsonTree->find("wavelength");
    if (wvlObject.has_value()) {
        auto wvlIt = wvlObject.value();
        if (std::holds_alternative<double>(wvlIt->second->value)) {
            input.hasSpectrum = false;
            input.wavelength = std::get<double>(wvlIt->second->value);
        }
        else if (std::holds_alternative<Json::JsonObject*>(wvlIt->second->value)) {
            double peakWvl, fwhm;
            auto spectrumObject = std::get<Json::JsonObject*>(wvlIt->second->value);
            auto spectrumTypeIt = spectrumObject->find("spectrum");
            if (spectrumTypeIt != spectrumObject->end()) {
                input.hasSpectrum = true;
                if (std::holds_alternative<std::string>(spectrumTypeIt->second->value)) {
                    std::string spectrumTypeLabel = std::get<std::string>(spectrumTypeIt->second->value);
                    if (spectrumTypeLabel != "Gaussian") {throw std::runtime_error("Invalid spectrum type!");}
                    else {
                        auto peakWvlIt = spectrumObject->find("peak wavelength");
                        if (peakWvlIt != spectrumObject->end()) {
                            if (std::holds_alternative<double>(peakWvlIt->second->value)) {
                                peakWvl = std::get<double>(peakWvlIt->second->value);
                            }
                            else {throw std::runtime_error("Invalid peak wavelength value.");}
                        }
                        else {throw std::runtime_error("You need to specify the peak wavelength!");}
                    }
                        auto fwhmIt = spectrumObject->find("fwhm");
                        if (fwhmIt != spectrumObject->end()) {
                            if (std::holds_alternative<double>(fwhmIt->second->value)) {
                                fwhm = std::get<double>(fwhmIt->second->value);
                            }
                            else {throw std::runtime_error("Invalid fwhm value!");}
                        }
                        else {throw std::runtime_error("You need to specify the fwhm!");}
                }
                else {throw std::runtime_error("Invalid spectrum format!");}
            }
            else {throw std::runtime_error("You need to specify a spectrum object!");}
            input.spectrum = GaussianSpectrum(450, 700, peakWvl, 2*fwhm);
        }
        else {throw std::runtime_error("Invalid wavelength entry!");}
    }
    else {throw std::runtime_error("You need to provide a stack!");}
}

const Input& ConfigurationManager::getInput() const {
    return input;
}

SimulationManager::SimulationManager(const std::string& filename):
    _configurator(filename) {
        _configurator.configure();
    }

std::unique_ptr<BaseSolver> SimulationManager::create() {
    Input input = _configurator.getInput();
    if (input.mode==SolverMode::Simulation)  {
        if (!input.hasDipoleDistribution && !input.hasSpectrum) {
            return std::make_unique<Simulation>(SimulationMode::AngleSweep,
                                                input.layers,
                                                input.emitterPosition,
                                                input.wavelength,
                                                0.0,
                                                90.0);
        }
        else {
            if (!input.hasDipoleDistribution) {
                return std::make_unique<Simulation>(SimulationMode::AngleSweep,
                                                    input.layers,
                                                    input.emitterPosition,
                                                    input.spectrum,
                                                    0.0,
                                                    90.0);
            }
            else if (input.hasDipoleDistribution && input.hasSpectrum) {
                return std::make_unique<Simulation>(SimulationMode::AngleSweep,
                                                    input.layers,
                                                    input.dipoleDist,
                                                    input.spectrum,
                                                    0.0,
                                                    90.0);
            }
        }
    }
    else if (input.mode==SolverMode::Fitting) {
        throw std::runtime_error("Not Implemented yet!");
    }
    else {throw std::runtime_error("Invalid mode!");}
}
