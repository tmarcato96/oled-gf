#pragma once

#include <string>
#include <optional>
#include <vector>
#include <Eigen/Core>
#include <fstream>

#include <jsonsimplecpp\node.hpp>
#include <jsonsimplecpp\parser.hpp>

#include <forwardDecl.hpp>
#include <matlayer.hpp>
#include <basesolver.hpp>

namespace Data {

    Matrix loadFromFile(const std::string& filepath, size_t ncols, char delimiter='\t');

    struct JSONinput{
        public:
            std::map<int, Layer> layerMap;
            std::map<double, double> fitData;
            std::optional<double> alpha;
        
        private:
            struct VisitorPolicy {
                
            };

            VisitorPolicy policy;
            std::string _filepath;
            std::unique_ptr<Json::JsonNode<Json::PrintVisitor>> _root;
            std::unique_ptr<Json::JsonNode<Json::PrintVisitor>> setRoot();

    };
    
    struct CSVinput{
        public:
            std::map<int, Layer> layerMap;
            std::map<double, double> fitData;
            std::optional<double> alpha;

        private:
    };

    template<typename T>
    class Reader {};

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
            std::unique_ptr<Json::JsonNode<Json::PrintVisitor>> _root;
            std::unique_ptr<Json::JsonNode<Json::PrintVisitor>> setRoot();
            Reader<JSONinput> reader;

            void setSolverMode() override;
        
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