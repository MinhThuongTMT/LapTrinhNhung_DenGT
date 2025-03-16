# 🛑 HỆ THỐNG ĐÈN GIAO THÔNG SỬ DỤNG NUCLEO-F429ZI 🚦  

## 📌 Giới thiệu  
Hệ thống đèn giao thông này được xây dựng trên nền tảng **NUCLEO-F429ZI**, cho phép hoạt động linh hoạt giữa:  
✅ **Chế độ tự động**  
✅ **Chế độ thủ công**  
✅ **Chế độ cài đặt thời gian**  

Hệ thống giúp mô phỏng đèn giao thông thực tế, đồng thời cho phép người dùng điều chỉnh thời gian đèn dễ dàng.  

🎥 **Video mô phỏng hệ thống**:  
[![Video Hệ Thống Đèn Giao Thông](https://img.youtube.com/vi/YOUR_VIDEO_ID/maxresdefault.jpg)](https://www.youtube.com/watch?v=YOUR_VIDEO_ID)  
*(Nhấn vào ảnh để xem video!)*  

---

## 🔧 Quy trình hoạt động  

### ✅ Chế độ tự động (Mặc định)  
- 🟢 **Đèn Xanh** sáng trong **2 giây**  
- 🔴 **Đèn Đỏ** sáng trong **1 giây**  
- 🟡 **Đèn Vàng** sáng trong **0.5 giây**  

### 🔄 Chuyển sang chế độ thủ công  
Nhấn **`*`**, sau đó nhập **mật khẩu `7373`** và nhấn **`#`** để kích hoạt chế độ thủ công.  

### 🔄 Chuyển lại chế độ tự động  
Nhấn **`*`**, sau đó nhập **mật khẩu `8888`** và nhấn **`#`** để quay lại chế độ tự động.  

### ⚙️ Chế độ cài đặt thời gian đèn  
1. Nhấn **`*`**, nhập **mật khẩu `0000`**, sau đó nhấn **`#`** để vào chế độ cài đặt.  
2. Chọn các phím tương ứng với đèn muốn điều chỉnh:  
   - 🟢 **B** ➝ Đèn Xanh  
   - 🟡 **C** ➝ Đèn Vàng  
   - 🔴 **D** ➝ Đèn Đỏ  
3. Nhập thời gian mong muốn (đơn vị: giây).  
4. Nhấn **`A`** để xác nhận.  

### 🔄 Cập nhật hệ thống  
Sau khi cài đặt thời gian, nhấn **`*`** để cập nhật hệ thống.  

---

## ⚡ Phần cứng sử dụng  
- 🛠 **NUCLEO-F429ZI** (MCU ARM Cortex-M4)  
- 💡 **Đèn LED RGB** hoặc **đèn tín hiệu giao thông mô phỏng**  
- 🎛 **Bàn phím số (Keypad 4x4)** để nhập lệnh  
- 🔌 **Nguồn cấp 5V/3.3V**  

---

## 📌 Ứng dụng  
✅ **Mô phỏng hệ thống đèn giao thông thực tế**  
✅ **Dùng trong giảng dạy về hệ thống nhúng và vi điều khiển**  
✅ **Có thể mở rộng để điều khiển đèn giao thông thực tế**  

🚀 **Dự án sẵn sàng để triển khai và phát triển thêm các tính năng mở rộng!**  

---

📩 *Mọi ý kiến đóng góp hoặc hỗ trợ, vui lòng liên hệ nhóm phát triển!*  
