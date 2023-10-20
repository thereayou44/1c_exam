#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>

class DirectoryComparator {
 public:
  DirectoryComparator(uint8_t similarity_threshold, std::string directory1, std::string directory2)
      : directory1_(directory1)
      , directory2_(directory2) {similarity_threshold_ = static_cast<double>(similarity_threshold) / 100;}

  void CompareDirectories() {
    // Найти и вывести идентичные файлы
    FindIdenticalFiles();

    PrintIdenticalFiles();

    // Найти похожие файлы
    FindSimilarFiles();

    // Вывести похожие файлы
    PrintSimilarFiles();

    // Найти файлы, которые присутствуют только в одной директории
    FindUniqueFiles();
  }

 private:
  double similarity_threshold_; // if similarity is equal to threshold, files are equal
  std::string directory1_;
  std::string directory2_;
  std::unordered_map<uint64_t, std::vector<std::string>> hashTable1_;
  std::unordered_map<uint64_t, std::vector<std::string>> hashTable2_;

  std::unordered_map<std::string, std::vector<std::string>> equal_files;   // список идентичных файлов
  std::unordered_map<std::string, std::vector<std::string>> similar_files; // список похожих файлов

  size_t calculateFileHash(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
      return 0;
    }

    uint64_t hash = 0;
    const int8_t kPbase = 31;
    const uint64_t mod = 1e9;
    char buffer[1024];
    while (!file.eof()) {
      file.read(buffer, sizeof(buffer));
      size_t bytesRead = file.gcount();
      for (size_t i = 0; i < bytesRead; ++i) {
        hash = ((hash * kPbase) % mod + static_cast<uint8_t>(buffer[i])) % mod;
      }
    }
    return hash;
  }

  // заполнение hash-таблицы
  void populateHashTable(const std::string& directory, std::unordered_map<size_t, std::vector<std::string>>& hashTable) {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
      size_t hash = calculateFileHash(entry.path().string());
      hashTable[hash].push_back(entry.path().string());
    }
  }

  void FindIdenticalFiles() {
    populateHashTable(directory1_, hashTable1_);
    populateHashTable(directory2_, hashTable2_);

    for (const auto& file1 : hashTable1_) {
      size_t hash = file1.first;
      for (std::string file_name : file1.second) {
        equal_files[file_name] = hashTable2_[hash];
      }
    }
    for (const auto& file2 : hashTable2_) {
      size_t hash = file2.first;
      for (std::string file_name : file2.second) {
        equal_files[file_name] = hashTable1_[hash];
      }
    }
  }

  void PrintIdenticalFiles() {
    for (const auto& entry : std::filesystem::directory_iterator(directory1_)) {
      std::string file_name = entry.path();
      for (std::string equal_file_name : equal_files[file_name]) {
        std::cout << "File " << file_name << " and File " << equal_file_name << " are identical\n";
      }
    }
  }

  uint64_t CalculateLevenshteinDistance(const std::string& str1, const std::string& str2) {
    uint64_t length1 = str1.length();
    uint64_t length2 = str2.length();

    std::vector<std::vector<uint64_t>> distances(length1 + 1, std::vector<uint64_t>(length2 + 1, 0));

    for (uint64_t i = 0; i <= length1; ++i) {
      distances[i][0] = i;
    }

    for (uint64_t j = 0; j <= length2; ++j) {
      distances[0][j] = j;
    }

    for (uint64_t i = 1; i <= length1; ++i) {
      for (uint64_t j = 1; j <= length2; ++j) {
        int cost = (str1[i - 1] == str2[j - 1]) ? 0 : 1;
        distances[i][j] = std::min({
                                       distances[i - 1][j] + 1,
                                       distances[i][j - 1] + 1,
                                       distances[i - 1][j - 1] + cost
                                   });
      }
    }

    return distances[length1][length2];
  }

  bool IsSimilar(const std::string& file1, const std::string& file2) {
    // сравнение двух файлов на схожесть
    if (std::find(similar_files[file1].begin(), similar_files[file1].end(), file2) != similar_files[file1].end()) {
      return true;
    }
    std::ifstream stream1(file1);
    std::ifstream stream2(file2);

    if (!stream1.is_open() || !stream2.is_open()) {
      return false; // если нет доступа к содержимому, то файл будет уникальным,
      // т.к никто не будет схожим с ним и идентичным тоже)
    }

    std::string content1((std::istreambuf_iterator<char>(stream1)), std::istreambuf_iterator<char>());
    std::string content2((std::istreambuf_iterator<char>(stream2)), std::istreambuf_iterator<char>());

    size_t len1 = content1.size();
    size_t len2 = content2.size();

    size_t maxLength = std::max(len1, len2);

    uint64_t lev_dist = CalculateLevenshteinDistance(content1, content2);

    double similarity = 1 - static_cast<double>(lev_dist) / maxLength;
    if (similarity >= similarity_threshold_ && similarity != 1) {
      similar_files[file1].push_back(file2);
      similar_files[file2].push_back(file1);

      for (std::string eq_file : equal_files[file1]) {
        similar_files[eq_file].push_back(file2);
        similar_files[file2].push_back(eq_file);
      }

      for (std::string eq_file : equal_files[file2]) {
        similar_files[eq_file].push_back(file1);
        similar_files[file1].push_back(eq_file);
      }
      return true;
    }
    return false;
  }

  void FindSimilarFiles() {
    for (const auto& entry1 : std::filesystem::directory_iterator(directory1_)) {
      for (const auto& entry2 : std::filesystem::directory_iterator(directory2_)) {
        std::string file1 = entry1.path();
        std::string file2 = entry2.path();
        IsSimilar(file1, file2);
      }
    }
  }

  void PrintSimilarFiles() {
    for (const auto& entry : std::filesystem::directory_iterator(directory1_)) {
      std::string file_name = entry.path();
      for (std::string similar_file_name : similar_files[file_name]) {
        std::cout << "File " << file_name << " and File " << similar_file_name << " are similar\n";
      }
    }
  }

  void FindUniqueFiles() {
    for (const auto& entry : std::filesystem::directory_iterator(directory1_)) {
      std::string file_name = entry.path();
      if (equal_files[file_name].size() == 0 && similar_files[file_name].size() == 0) {
        std::cout << "File " << file_name << " is unique\n";
      }
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory2_)) {
      std::string file_name = entry.path();
      if (equal_files[file_name].size() == 0 && similar_files[file_name].size() == 0) {
        std::cout << "File " << file_name << " is unique\n";
      }
    }
  }
};

int main() {
  std::string directory1 = "test/dir1";
  std::string directory2 = "test/dir2";
  uint8_t threshold = 70;

  DirectoryComparator comp(threshold, directory1, directory2);

  comp.CompareDirectories();
}
