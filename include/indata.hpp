#pragma once

#include <string>
#include <optional>
#include <vector>
#include <Eigen/Core>
#include <fstream>

#include <jsonsimplecpp\node.hpp>
#include <jsonsimplecpp\parser.hpp>

#include <visitor.hpp>
#include <fileutils.hpp>
#include <forwardDecl.hpp>
#include <matlayer.hpp>
#include <basesolver.hpp>

namespace Data {

    //filepolicy is Json::JsonNode<ConfigVisitor>
    template<typename FilePolicy=Json::JsonNode<ConfigVisitor>>
    class Reader {
        private:
            std::map<int, Layer> _layerMap;
            std::map<double, double> _fitData;
            std::optional<double> _alpha;
            SolverMode _smode;
            std::unique_ptr<Json::JsonNode<ConfigVisitor>> _rootPtr;
            std::unique_ptr<ConfigVisitor> _visitor;

            
            const std::string _filepath;
            Json::JsonNode<ConfigVisitor> _policy;
        
            void parseFile() {
                _policy.setVisitor(_visitor);
                _policy.parse();
                auto resPtr = const_cast<Json::JsonNode<ConfigVisitor>*>(_policy.getJsonTree());
                _rootPtr{resPtr}; //triple check
            }
        
        public:
            Reader(const std::string& filepath) :
            _policy{filepath},
            _visitor{new ConfigVisitor(_layerMap, _fitData, _alpha)}
             {
                parseFile();
                
                if (_alpha.has_value()) _smode = SolverMode::fitting;
                else{ _smode = SolverMode::simulation;}
            }

            SolverMode get_mode(){return _smode;}

            std::unique_ptr<BaseSolver> makeSolver() {
                
                std::unique_ptr<BaseSolver> solverPtr;
                if (_smode == SolverMode::fitting) solverPtr = std::make_unique<Fitting>(_fitData, _layerMap, 0.0, 0.0, 170)
                else{ solverPtr = std::make_unique<Simulation>(SimulationMode::AngleSweep, _layerMap, 0.0, 0.0, 0.0, 170.0)}
                
                return std::move(solverPtr);
            }
    };

    class Importer {
        protected:
            virtual void setSolverMode() = 0;
            SolverMode _mode;

            Importer(const std::string& filepath);
            Importer(const std::string& filepath, SolverMode mode);
            const std::string& _filepath;

        public:
            virtual std::unique_ptr<BaseSolver> solverFromFile() = 0;
            virtual ~Importer() = default;
    };

    class INIimporter : public Importer {
        protected:
            void setSolverMode() override;
        
        public:
            std::unique_ptr<BaseSolver> solverFromFile() override;
            INIimporter(const std::string& filepath);
            INIimporter(const std::string& filepath, SolverMode mode);
    };

    class JSONimporter : public Importer {
        private:
            void setSolverMode() override;
            Reader<Json::JsonNode<ConfigVisitor>> _reader;
        
        public:
            std::unique_ptr<BaseSolver> solverFromFile() override;

            JSONimporter(const std::string& filepath);
            JSONimporter(const std::string& filepath, SolverMode mode);
    };


    enum class SolverMode{simulation, fitting, automatic};
    enum class FileFormat{CSV, JSON, automatic};

    class ImportManager {
        private:
            const std::string& _filepath;
            FileFormat _ftype;
            SolverMode _smode;
            std::ifstream _fin;
            
            void autoSetFileFormat();

        public:
            ImportManager(const std::string& filepath);
            ImportManager(const std::string& filepath, FileFormat);
            ImportManager(const std::string& filepath, SolverMode);
            ImportManager(const std::string& filepath, FileFormat, SolverMode);

            std::unique_ptr<Importer> makeImporter();
    };
}