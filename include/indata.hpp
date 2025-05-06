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
    template<typename FilePolicy>
    class Reader {
    private:
        std::map<int, Layer> _layerMap;
        std::map<double, double> _fitData;
        std::optional<double> _alpha;
    
        std::string _filepath;
        FilePolicy _policy;
    
        void parseFile() {
            _policy(_filepath);
            _policy.parse();
            _policy.
        }
    
    public:
        Reader() {
            parseFile();
        }

        std::unique_ptr<BaseSolver> makeSolver();
    };

    class Importer {
        protected:
            virtual void setSolverMode() = 0;
            SolverMode _mode;

            Importer(const std::string& filepath);
            Importer(const std::string& filepath, SolverMode mode);
            const std::string& _filepath;
            std::ifstream _fin;

        public:
            std::unique_ptr<BaseSolver> solverFromFile();
            virtual ~Importer() = default;
    };

    class CSVimporter : public Importer {
        protected:
            void setSolverMode() override;
        
        public:
            std::unique_ptr<BaseSolver> solverFromFile() override;
            CSVimporter(const std::string& filepath);
            CSVimporter(const std::string& filepath, SolverMode mode);
    };

    class JSONimporter : public Importer {
        private:
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