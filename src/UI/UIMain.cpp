#include "UIMain.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include "ActiveClip.h"
#include "DetectedProblems.h"
#include "Jobs.h"
#include "OpenAnimationReplacer.h"
#include "Parsing.h"
#include "UICommon.h"
#include "UIManager.h"

namespace UI
{
	bool UIMain::ShouldDrawImpl() const
	{
		return UIManager::GetSingleton().bShowMain;
	}

	void UIMain::DrawImpl()
	{
		// disable idle camera rotation
		if (const auto playerCamera = RE::PlayerCamera::GetSingleton()) {
			playerCamera->GetRuntimeData2().idleTimer = 0.f;
		}

		//ImGui::ShowDemoWindow();

		SetWindowDimensions(0.f, 0.f, 850.f, -1, WindowAlignment::kCenterLeft);

		const auto title = std::format("Open Animation Replacer {}.{}.{}", Plugin::VERSION.major(), Plugin::VERSION.minor(), Plugin::VERSION.patch());
		if (ImGui::Begin(title.data(), &UIManager::GetSingleton().bShowMain, ImGuiWindowFlags_NoCollapse)) {
			if (ImGui::BeginTable("EvaluateForReference", 2, ImGuiTableFlags_None)) {
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				static char formIDBuf[9] = "";
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5);

				if (const auto consoleRefr = Utils::GetConsoleRefr()) {
					ImGui::BeginDisabled();
					std::string formID = std::format("{:08X}", consoleRefr->GetFormID());
					ImGui::InputText("Referans için değerlendir", formID.data(), ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);
					ImGui::EndDisabled();
				} else {
					ImGui::InputTextWithHint("Referans için değerlendir", "FormID...", formIDBuf, IM_ARRAYSIZE(formIDBuf), ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase, &ReferenceInputTextCallback);
				}

				ImGui::SameLine();
				UICommon::HelpMarker("Animasyonları ve koşulları bu referansa ait mevcut sonuçlarına göre renklendirmek için oyun konsolunda bir referans seçin veya referansın FormID değerini girin. Baştaki sıfırları yazmanıza gerek yoktur. (Oyuncunun FormID değeri 14'tür).");

				if (const auto refr = UIManager::GetSingleton().GetRefrToEvaluate()) {
					ImGui::TableSetColumnIndex(1);
					ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(UICommon::SUCCESS_BG_COLOR));
					ImGui::TextUnformatted(refr->GetDisplayFullName());
				}

				ImGui::EndTable();
			}

			//ImGui::Spacing();

			const auto& style = ImGui::GetStyle();
			const float bottomBarHeight = ImGui::GetTextLineHeight() + style.FramePadding.y * 4 + style.ItemSpacing.y * 4;
			if (ImGui::BeginChild("Tabs", ImVec2(0.f, -bottomBarHeight), true)) {
				if (ImGui::BeginTabBar("TabBar")) {
					if (ImGui::BeginTabItem("Değiştirici Modlar")) {
						DrawReplacerMods();
						ImGui::EndTabItem();
					}
					if (ImGui::BeginTabItem("Yedek Animasyonlar")) {
						DrawReplacementAnimations();
						ImGui::EndTabItem();
					}

					ImGui::EndTabBar();
				}
			}
			ImGui::EndChild();

			if (_commentState.bShouldOpen) {
				ImGui::OpenPopup("Edit comment##popup");
				_commentState.bShouldOpen = false;
			}

			if (_commentState.Valid()) {
				ImGui::SetNextWindowSize(ImVec2(280.f, 0.f));
				if (ImGui::BeginPopupModal("Edit comment##popup"), nullptr, ImGuiWindowFlags_NoDecoration) {
					if (ImGui::IsWindowAppearing()) {
						ImGui::SetKeyboardFocusHere();
					}

					float avail = ImGui::GetContentRegionAvail().x;
					float buttonWidth = (avail - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

					bool bEdited = _commentState.HasUnsavedChanges();

					ImGui::SetNextItemWidth(avail);

					if (ImGui::InputTextWithHint("##comment", "Yorum ekle...", &_commentState.buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
						if (bEdited) {
							_commentState.Save();
							ImGui::CloseCurrentPopup();
						}
					}

					if (ImGui::Button("İptal", ImVec2(buttonWidth, 0.f))) {
						_commentState.Clear();
						ImGui::CloseCurrentPopup();
					}

					ImGui::SameLine();

					ImGui::BeginDisabled(!bEdited);
					if (ImGui::Button("Kaydet", ImVec2(buttonWidth, 0.f))) {
						_commentState.Save();
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();

					ImGui::EndPopup();
				}
			}

			const std::string animationLogButtonName = "Animasyon Günlüğü";
			const float animationLogButtonWidth = (ImGui::CalcTextSize(animationLogButtonName.data()).x + style.FramePadding.x * 2 + style.ItemSpacing.x);

			const std::string animationEventLogButtonName = "Olay Günlüğü";
			const float animationEventLogButtonWidth = (ImGui::CalcTextSize(animationEventLogButtonName.data()).x + style.FramePadding.x * 2 + style.ItemSpacing.x);

			const std::string settingsButtonName = _bShowSettings ? "Ayarlar <" : "Ayarlar >";
			const float settingsButtonWidth = (ImGui::CalcTextSize(settingsButtonName.data()).x + style.FramePadding.x * 2 + style.ItemSpacing.x);

			// Bottom bar
			if (ImGui::BeginChild("BottomBar", ImVec2(ImGui::GetContentRegionAvail().x - animationLogButtonWidth - animationEventLogButtonWidth - settingsButtonWidth, 0.f), true)) {
				ImGui::AlignTextToFramePadding();
				// Status text

				auto& problems = DetectedProblems::GetSingleton();

				const std::string_view problemText = problems.GetProblemMessage();
				using Severity = DetectedProblems::Severity;

				if (problems.GetProblemSeverity() > Severity::kNone) {
					// Problems found
					switch (problems.GetProblemSeverity()) {
					case Severity::kWarning:
						ImGui::PushStyleColor(ImGuiCol_Button, UICommon::WARNING_BUTTON_COLOR);
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UICommon::WARNING_BUTTON_HOVERED_COLOR);
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, UICommon::WARNING_BUTTON_ACTIVE_COLOR);
						break;
					case Severity::kError:
						ImGui::PushStyleColor(ImGuiCol_Button, UICommon::ERROR_BUTTON_COLOR);
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UICommon::ERROR_BUTTON_HOVERED_COLOR);
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, UICommon::ERROR_BUTTON_ACTIVE_COLOR);
						break;
					}

					if (ImGui::Button(problemText.data(), ImGui::GetContentRegionAvail())) {
						if (problems.CheckForProblems()) {
							ImGui::OpenPopup("Problems");
						}
					}

					ImGui::PopStyleColor(3);

					const auto viewport = ImGui::GetMainViewport();
					ImGui::SetNextWindowPos(ImVec2(viewport->Size.x * 0.5f, viewport->Size.y * 0.5f), ImGuiCond_None, ImVec2(0.5f, 0.5f));

					float height = 30.f;
					if (problems.IsOutdated()) {
						height += 100.f;
					}
					if (problems.HasMissingPlugins()) {
						height += 100.f;
						height += problems.NumMissingPlugins() * ImGui::GetTextLineHeightWithSpacing();
					}
					if (problems.HasSubModsWithInvalidEntries()) {
						height += 200.f;
						height += problems.NumSubModsSharingPriority() * ImGui::GetTextLineHeightWithSpacing();
					}
					if (problems.HasSubModsSharingPriority()) {
						height += 200.f;
						height += problems.NumSubModsSharingPriority() * ImGui::GetTextLineHeightWithSpacing();
					}

					const float buttonHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().FramePadding.y * 2.f;
					height += buttonHeight;

					height = std::clamp(height, 600.f, ImGui::GetIO().DisplaySize.y);

					ImGui::SetNextWindowSize(ImVec2(800.f, height));
					if (ImGui::BeginPopupModal("Problems", nullptr, ImGuiWindowFlags_NoResize)) {
						auto childSize = ImGui::GetContentRegionAvail();
						childSize.y -= buttonHeight;
						if (ImGui::BeginChild("ProblemsContent", childSize)) {
							bool bShouldDrawSeparator = false;
							if (problems.IsOutdated()) {
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::ERROR_TEXT_COLOR);
								ImGui::TextWrapped("HATA: En az bir değiştirici mod, Open Animation Replacer'ın daha yeni bir sürümünü gerektiren koşullara sahip.\nMod düzgün çalışmayacaktır. Lütfen Open Animation Replacer'ı güncelleyin!");
								ImGui::PopStyleColor();

								bShouldDrawSeparator = true;
							}

							if (problems.HasMissingPlugins()) {
								if (bShouldDrawSeparator) {
									ImGui::Spacing();
									ImGui::Spacing();
									ImGui::Separator();
									ImGui::Spacing();
									ImGui::Spacing();
								}
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::ERROR_TEXT_COLOR);
								ImGui::TextWrapped("HATA: En az bir değiştirici mod, Open Animation Replacer'ın kendisinde bulunmayan, eksik veya eski bir Open Animation Replacer API eklentisi tarafından eklenen koşullara sahip.\nMod düzgün çalışmayacaktır. Lütfen gerekli eklentiyi indirin veya güncelleyin!");
								ImGui::PopStyleColor();
								ImGui::Spacing();
								DrawMissingPlugins();

								bShouldDrawSeparator = true;
							}

							if (problems.HasInvalidPlugins()) {
								if (bShouldDrawSeparator) {
									ImGui::Spacing();
									ImGui::Spacing();
									ImGui::Separator();
									ImGui::Spacing();
									ImGui::Spacing();
								}
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::ERROR_TEXT_COLOR);
								ImGui::TextWrapped("HATA: En az bir değiştirici mod, Open Animation Replacer'ın kendisinde bulunmayan ancak düzgün başlatılamamış gibi görünen bir Open Animation Replacer API eklentisi tarafından eklenen koşullara sahip.\nMod düzgün çalışmayacaktır. Lütfen gerekli eklentiyi ve tüm bağımlılıklarını doğru şekilde yüklediğinizden emin olun!");
								ImGui::PopStyleColor();
								ImGui::Spacing();
								DrawInvalidPlugins();

								bShouldDrawSeparator = true;
							}

							if (problems.HasReplacerModsWithInvalidEntries()) {
								if (bShouldDrawSeparator) {
									ImGui::Spacing();
									ImGui::Spacing();
									ImGui::Separator();
									ImGui::Spacing();
									ImGui::Spacing();
								}
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::ERROR_TEXT_COLOR);
								ImGui::TextWrapped("HATA: En az bir değiştirici mod geçersiz koşullara sahip.\nMod düzgün çalışmayacaktır. Lütfen değiştirici modun koşullarını kontrol edin ve yayınlanmış bir güncelleme olup olmadığına bakın!");
								ImGui::PopStyleColor();
								ImGui::Spacing();
								DrawReplacerModsWithInvalidConditions();

								bShouldDrawSeparator = true;
							}

							if (problems.HasSubModsWithInvalidEntries()) {
								if (bShouldDrawSeparator) {
									ImGui::Spacing();
									ImGui::Spacing();
									ImGui::Separator();
									ImGui::Spacing();
									ImGui::Spacing();
								}
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::ERROR_TEXT_COLOR);
								ImGui::TextWrapped("HATA: En az bir alt mod geçersiz koşullara sahip.\nMod düzgün çalışmayacaktır. Lütfen değiştirici modun koşullarını kontrol edin ve yayınlanmış bir güncelleme olup olmadığına bakın!");
								ImGui::PopStyleColor();
								ImGui::Spacing();
								DrawSubModsWithInvalidConditions();

								bShouldDrawSeparator = true;
							}

							if (problems.HasSubModsSharingPriority()) {
								if (bShouldDrawSeparator) {
									ImGui::Spacing();
									ImGui::Spacing();
									ImGui::Separator();
									ImGui::Spacing();
									ImGui::Spacing();
								}
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::WARNING_TEXT_COLOR);
								ImGui::TextWrapped("UYARI: Aşağıdaki modlar çakışan önceliklere sahip.\nAynı animasyonları değiştiriyorlarsa bu durum beklenmeyen davranışlara yol açabilir.");
								ImGui::PopStyleColor();
								ImGui::Spacing();
								DrawConflictingSubMods();

								bShouldDrawSeparator = true;
							}
						}
						ImGui::EndChild();

						constexpr float buttonWidth = 120.f;
						ImGui::SetCursorPosX((ImGui::GetWindowSize().x - buttonWidth) * 0.5f);
						ImGui::SetItemDefaultFocus();
						if (ImGui::Button("Tamam", ImVec2(buttonWidth, 0))) {
							ImGui::CloseCurrentPopup();
						}
						ImGui::EndPopup();
					}
				} else {
					ImGui::TextUnformatted(problemText.data());
				}
			}
			ImGui::EndChild();

			// Animation log button
			ImGui::SameLine(ImGui::GetWindowWidth() - animationLogButtonWidth - animationEventLogButtonWidth - settingsButtonWidth);
			if (ImGui::Button(animationLogButtonName.data(), ImVec2(0.f, bottomBarHeight - style.ItemSpacing.y))) {
				Settings::bEnableAnimationLog = !Settings::bEnableAnimationLog;
				Settings::WriteSettings();
			}

			// Animation event log button
			ImGui::SameLine(ImGui::GetWindowWidth() - animationEventLogButtonWidth - settingsButtonWidth);
			if (ImGui::Button(animationEventLogButtonName.data(), ImVec2(0.f, bottomBarHeight - style.ItemSpacing.y))) {
				Settings::bEnableAnimationEventLog = !Settings::bEnableAnimationEventLog;
				Settings::WriteSettings();
			}

			// Settings button
			ImGui::SameLine(ImGui::GetWindowWidth() - settingsButtonWidth);
			if (ImGui::Button(settingsButtonName.data(), ImVec2(0.f, bottomBarHeight - style.ItemSpacing.y))) {
				_bShowSettings = !_bShowSettings;
			}
		}

		const auto windowContentMax = ImGui::GetWindowContentRegionMax();
		const auto windowPos = ImGui::GetWindowPos();
		const auto settingsPos = ImVec2(windowPos.x + windowContentMax.x + 7.f, windowPos.y + windowContentMax.y + 8.f);

		ImGui::End();

		if (_bShowSettings) {
			DrawSettings(settingsPos);
		}
	}

	void UIMain::OnOpen()
	{
		UIManager::GetSingleton().AddInputConsumer();

		auto& detectedProblems = DetectedProblems::GetSingleton();
		detectedProblems.CheckForSubModsSharingPriority();
		detectedProblems.CheckForSubModsWithInvalidEntries();
	}

	void UIMain::OnClose()
	{
		UIManager::GetSingleton().RemoveInputConsumer();
		_commentState.Clear();
	}

	void UIMain::DrawSettings(const ImVec2& a_pos)
	{
		ImGui::SetNextWindowPos(a_pos, ImGuiCond_None, ImVec2(0.f, 1.f));

		if (ImGui::Begin("Settings", &_bShowSettings, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
			// UI settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Arayüz Ayarları");
			ImGui::Spacing();

			if (UICommon::InputKey("Menü tuşu", Settings::uToggleUIKeyData)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Arayüzü açıp kapatacak tuşu belirleyin.");

			ImGui::Spacing();

			if (ImGui::Checkbox("Karşılama başlığını göster", &Settings::bShowWelcomeBanner)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Başlangıçta karşılama başlığının gösterilmesini etkinleştirir.");

			static float tempScale = Settings::fUIScale;
			ImGui::SliderFloat("Arayüz ölçeği", &tempScale, 0.5f, 2.f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SameLine();
			UICommon::HelpMarker("Arayüz ölçeğini ayarlar.");
			ImGui::SameLine();
			ImGui::BeginDisabled(tempScale == Settings::fUIScale);
if (ImGui::Button("Uygula##UIScale")) {
				Settings::fUIScale = tempScale;
				Settings::WriteSettings();
			}
			ImGui::EndDisabled();

			ImGui::Spacing();
			ImGui::Separator();

			// General settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Genel Ayarlar");
			ImGui::Spacing();

			constexpr uint16_t animLimitMin = 0x4000;
			const uint16_t animLimitMax = Settings::GetMaxAnimLimit();
			if (ImGui::SliderScalar("Animasyon sınırı", ImGuiDataType_U16, &Settings::uAnimationLimit, &animLimitMin, &animLimitMax, "%d", ImGuiSliderFlags_AlwaysClamp)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Davranış projesi (behavior project) başına animasyon sınırını belirleyin. Yığın boyutunu (heap size) artırmadan bu değeri çok yüksek ayarlarsanız oyun çökecektir. Oyun, burada belirlenen üst sınırın ötesindeki animasyonları oynatamaz; .ini dosyası üzerinden bunu aşmaya çalışmanın bir anlamı yoktur.");

			constexpr uint32_t heapMin = 0x20000000;
			constexpr uint32_t heapMax = 0x7FC00000;
			if (ImGui::SliderScalar("Havok yığın boyutu (heap size)", ImGuiDataType_U32, &Settings::uHavokHeapSize, &heapMin, &heapMax, "0x%X", ImGuiSliderFlags_AlwaysClamp)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Havok yığın boyutunu belirleyin. Oyun yeniden başlatıldıktan sonra etkili olur. (Orijinal/Vanilla değer: 0x20000000)");

			if (Settings::bDisablePreloading) {
				ImGui::BeginDisabled();
				bool bDummy = false;
				ImGui::Checkbox("Varsayılan davranışları ana menüde yükle", &bDummy);
				ImGui::EndDisabled();
			} else {
				if (ImGui::Checkbox("Varsayılan davranışları ana menüde yükle", &Settings::bLoadDefaultBehaviorsInMainMenu)) {
					Settings::WriteSettings();
				}
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Ana menüdeyken varsayılan erkek/kadın davranışlarını yüklemeye başlamak için etkinleştirin. Animasyon ön yüklemesi devre dışı bırakıldığında bir faydası olmadığı için yok sayılır.");

			constexpr uint32_t workerCountMin = 1;
			constexpr uint32_t workerCountMax = 32;
			if (ImGui::SliderScalar("Ayrıştırma iş parçacığı sayısı", ImGuiDataType_U32, &Settings::uParsingWorkerCount, &workerCountMin, &workerCountMax, "%d", ImGuiSliderFlags_AlwaysClamp)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Açılışta animasyon değiştirici modlar ayrıştırılırken kullanılacak iş parçacığı sayısı. Yüksek değerler, depolama çakışması (storage contention) nedeniyle daha yavaş olabilir. Oyun yeniden başlatıldıktan sonra etkili olur.");

			ImGui::Spacing();
			ImGui::Separator();

			// Duplicate filtering settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Mükerrer Filtreleme Ayarları");
			ImGui::Spacing();

			if (ImGui::Checkbox("Mükerrer animasyonları filtrele", &Settings::bFilterOutDuplicateAnimations)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Bir animasyon eklemeden önce mükerrer olup olmadığını kontrol etmek için etkinleştirin. Birden fazla değiştirici animasyonda bir animasyon bağlamasının yalnızca bir kopyası kullanılır. Değiştirici modlar aynı animasyonun farklı koşullara sahip birden fazla kopyasını kullanma eğiliminde olduğundan, bu işlem yüklenen animasyon sayısını ciddi oranda azaltabilir.");

			ImGui::Spacing();
			ImGui::Separator();

			// Animation queue progress bar settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Animasyon Kuyruğu İlerleme Çubuğu Ayarları");
			ImGui::Spacing();

			if (ImGui::Checkbox("Etkinleştir", &Settings::bEnableAnimationQueueProgressBar)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Animasyonlar yüklenirken bir ilerleme çubuğu gösterilmesini sağlar.");

			if (ImGui::SliderFloat("Ekranda kalma süresi", &Settings::fAnimationQueueLingerTime, 0.f, 10.f, "%.1f s", ImGuiSliderFlags_AlwaysClamp)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Tüm animasyonlar yüklendikten sonra çubuğun ekranda kalacağı süre.");

			ImGui::Spacing();
			ImGui::Separator();

			// Animation log settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Animasyon Günlüğü Ayarları");
			ImGui::Spacing();

			if (ImGui::SliderFloat("Günlük genişliği", &Settings::fAnimationLogWidth, 300.f, 1500.f, "%.0f", ImGuiSliderFlags_AlwaysClamp)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Animasyon günlüğü penceresinin genişliği.");

			constexpr uint32_t entriesMin = 1;
			constexpr uint32_t entriesMax = 20;
			if (ImGui::SliderScalar("Maksimum girdi sayısı", ImGuiDataType_U32, &Settings::uAnimationLogMaxEntries, &entriesMin, &entriesMax, "%d", ImGuiSliderFlags_AlwaysClamp)) {
				AnimationLog::GetSingleton().ClampLog();
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Animasyon günlüğündeki maksimum girdi sayısını belirleyin.");

			const char* logTypes[] = { "Değiştirildiğinde", "Olası değişikliklerle", "Tümü" };
			if (ImGui::SliderInt("Aktivasyon günlük modu", reinterpret_cast<int*>(&Settings::uAnimationActivateLogMode), 0, 2, logTypes[Settings::uAnimationActivateLogMode])) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Animasyon klibi aktivasyonunun günlüğe kaydedileceği koşulları belirleyin.");

			if (ImGui::SliderInt("Yankı (Echo) günlük modu", reinterpret_cast<int*>(&Settings::uAnimationEchoLogMode), 0, 2, logTypes[Settings::uAnimationEchoLogMode])) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Animasyon klibi yankısının (Echo) günlüğe kaydedileceği koşulları belirleyin. (Yankı, bir animasyon klibinin kendi kendine geçiş yapmasıdır. Döngüsel olmayan bazı kliplerde gerçekleşir)");

			if (ImGui::SliderInt("Döngü (Loop) günlük modu", reinterpret_cast<int*>(&Settings::uAnimationLoopLogMode), 0, 2, logTypes[Settings::uAnimationLoopLogMode])) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Animasyon klibi döngüsünün günlüğe kaydedileceği koşulları belirleyin.");

			if (ImGui::Checkbox("Yalnızca mevcut projeyi kaydet", &Settings::bAnimationLogOnlyActiveGraph)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Yalnızca aktif davranış grafiğindeki animasyon kliplerini kaydetmek için etkinleştirin - üçüncü şahıs kamerasındayken birinci şahıs animasyonlarını filtreler, vb.");

			if (ImGui::Checkbox("Günlük dosyasına yaz##AnimationLog", &Settings::bAnimationLogWriteToTextLog)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Klipleri 'Belgelerim\\My Games\\Skyrim Special Edition\\SKSE\\OpenAnimationReplacer.log' konumundaki dosyaya da kaydetmek için etkinleştirin.");

			ImGui::Spacing();
			ImGui::Separator();

			// Animation event log settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Animasyon Olay Günlüğü Ayarları");
			ImGui::Spacing();

			float tempSize[2] = { Settings::fAnimationEventLogWidth, Settings::fAnimationEventLogHeight };
			if (ImGui::SliderFloat2("Olay Günlüğü Boyutu", tempSize, 300.f, 1500.f, "%.0f")) {
				Settings::fAnimationEventLogWidth = tempSize[0];
				Settings::fAnimationEventLogHeight = tempSize[1];
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Animasyon olay günlüğünün genişliği ve yüksekliği.");

			if (ImGui::Checkbox("Günlük dosyasına yaz##EventLog", &Settings::bAnimationEventLogWriteToTextLog)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Animasyon olaylarını 'Belgelerim\\My Games\\Skyrim Special Edition\\SKSE\\OpenAnimationReplacer.log' konumundaki dosyaya da kaydetmek için etkinleştirin.");

			ImGui::Spacing();

			float tempOffset[2] = { Settings::fAnimationLogsOffsetX, Settings::fAnimationLogsOffsetY };
			if (ImGui::SliderFloat2("Günlük Konum Kaydırma (Offset)", tempOffset, 0.f, 500.f, "%.0f")) {
				Settings::fAnimationLogsOffsetX = tempOffset[0];
				Settings::fAnimationLogsOffsetY = tempOffset[1];
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Günlüklerin ekran köşesinden olan konum uzaklığı.");

			ImGui::Spacing();
			ImGui::Separator();

			// Workarounds
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Geçici Çözümler (Workarounds)");
			ImGui::SameLine();
			UICommon::HelpMarker("Bu ayarlar, bazı eski (Legacy) değiştirici modlardaki sorunlar için geçici çözümlerdir.");
			ImGui::Spacing();

			if (ImGui::Checkbox("Eski (Legacy) modlarda Rastgele (Random) koşullarının durum verilerini döngüde varsayılan olarak sıfırlama", &Settings::bLegacyKeepRandomResultsByDefault)) {
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Eski değiştirici modlar için Rastgele (Random) koşullarındaki \"Döngüde/yankıda sıfırlanmalı\" ayarını varsayılan olarak devre dışı bırakır. Bu, modların önceden beklendiği gibi davranmasını sağlar. Bu ayarın değiştirilmesi, oyun yeniden başlatıldıktan sonra etkili olur.");

			ImGui::Spacing();
			ImGui::Separator();

			// Experimental settings
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Deneysel Ayarlar");
			ImGui::SameLine();
			UICommon::HelpMarker("Bu ayarlar deneysel olarak kabul edilir ve sizde düzgün çalışmayabilir. Oyun yeniden başlatıldıktan sonra etkili olurlar.");
			ImGui::Spacing();

			if (ImGui::Checkbox("Ön yüklemeyi devre dışı bırak", &Settings::bDisablePreloading)) {
				if (Settings::bDisablePreloading) {
					Settings::bLoadDefaultBehaviorsInMainMenu = false;
				}
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Davranış ilk kez yüklendiğinde tüm animasyonların önceden yüklenmesini devre dışı bırakır. Bu önerilmez ve bu ayarın animasyon davranışlarında bazı farklılıklara yol açtığı kanıtlanmıştır.");

			if (ImGui::Checkbox("Animasyon sınırını artır", &Settings::bIncreaseAnimationLimit)) {
				Settings::ClampAnimLimit();
				Settings::WriteSettings();
			}
			ImGui::SameLine();
			UICommon::HelpMarker("Animasyon sınırını varsayılan değerin iki katına çıkarmak için etkinleştirin. Genellikle sorunsuz çalışmalıdır, ancak oyun kodunda yamalanması gereken bazı yerleri gözden kaçırmış olabileceğim için bu hala deneysel kabul edilmektedir. Sınırı aşmıyorsanız bunu etkinleştirmenin bir faydası yoktur.");
		}

		ImGui::End();
	}

	void UIMain::DrawMissingPlugins()
	{
		if (ImGui::BeginTable("MissingPlugins", 3, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersOuter)) {
			ImGui::TableSetupColumn("Eklenti", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Min. Gerekli", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("Mevcut", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			DetectedProblems::GetSingleton().ForEachMissingPlugin([&](auto& a_missingPlugin) {
				auto& missingPluginName = a_missingPlugin.first;
				auto& missingPluginVersion = a_missingPlugin.second;

				const REL::Version currentPluginVersion = OpenAnimationReplacer::GetSingleton().GetPluginVersion(missingPluginName);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				ImGui::TextUnformatted(missingPluginName.data());
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(missingPluginVersion.string("."sv).data());
				ImGui::TableSetColumnIndex(2);
				if (currentPluginVersion > 0) {
					ImGui::TextUnformatted(currentPluginVersion.string("."sv).data());
				} else {
					ImGui::TextUnformatted("Eksik");
				}
			});

			ImGui::EndTable();
		}
	}

	void UIMain::DrawInvalidPlugins()
	{
		if (ImGui::BeginTable("InvalidPlugins", 3, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersOuter)) {
			ImGui::TableSetupColumn("Eklenti", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Min. Gerekli", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupColumn("Mevcut", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			DetectedProblems::GetSingleton().ForEachMissingPlugin([&](auto& a_invalidPlugin) {
				auto& invalidPluginName = a_invalidPlugin.first;
				auto& invalidPluginVersion = a_invalidPlugin.second;

				const REL::Version currentPluginVersion = OpenAnimationReplacer::GetSingleton().GetPluginVersion(invalidPluginName);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				ImGui::TextUnformatted(invalidPluginName.data());
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(invalidPluginVersion.string("."sv).data());
				ImGui::TableSetColumnIndex(2);
				if (currentPluginVersion > 0) {
					ImGui::TextUnformatted(currentPluginVersion.string("."sv).data());
				} else {
					ImGui::TextUnformatted("Eksik");
				}
			});

			ImGui::EndTable();
		}
	}

	void UIMain::DrawConflictingSubMods() const
	{
		if (ImGui::BeginTable("ConflictingSubMods", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersOuter)) {
			ImGui::TableSetupColumn("Alt Mod", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Öncelik", ImGuiTableColumnFlags_WidthFixed, 100.f);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			bool bJustStarted = true;
			int32_t prevPriority = 0;

			DetectedProblems::GetSingleton().ForEachSubModSharingPriority([&](const SubMod* a_subMod) {
				const auto parentMod = a_subMod->GetParentMod();

				const auto nodeName = a_subMod->GetName();

				if (bJustStarted) {
					bJustStarted = false;
				} else if (prevPriority != a_subMod->GetPriority()) {
					ImGui::Spacing();
				}
				prevPriority = a_subMod->GetPriority();

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				const bool bOpen = ImGui::TreeNode(nodeName.data());
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(std::to_string(a_subMod->GetPriority()).data());
				if (bOpen) {
					if (parentMod) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(parentMod->GetName().data());
						UICommon::TextUnformattedEllipsis(a_subMod->GetPath().data());
					}
					ImGui::TreePop();
				}
			});

			ImGui::EndTable();
		}
	}

	void UIMain::DrawReplacerModsWithInvalidConditions() const
	{
		if (ImGui::BeginTable("ReplacerModsWithInvalidConditions", 1, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersOuter)) {
			ImGui::TableSetupColumn("Değiştirici mod", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			DetectedProblems::GetSingleton().ForEachReplacerModWithInvalidEntries([&](const ReplacerMod* a_replacerMod) {
				const auto replacerModName = a_replacerMod->GetName();

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(replacerModName.data());
			});

			ImGui::EndTable();
		}
	}

void UIMain::DrawSubModsWithInvalidConditions() const
	{
		if (ImGui::BeginTable("SubModsWithInvalidConditions", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_BordersOuter)) {
			ImGui::TableSetupColumn("Ana mod", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Alt mod", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableHeadersRow();

			DetectedProblems::GetSingleton().ForEachSubModWithInvalidEntries([&](const SubMod* a_subMod) {
				const auto subModName = a_subMod->GetName();
				const auto parentModName = a_subMod->GetParentMod()->GetName();

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);

				ImGui::TextUnformatted(parentModName.data());
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(subModName.data());
			});

			ImGui::EndTable();
		}
	}

	void UIMain::DrawReplacerMods()
	{
		static char nameFilterBuf[32] = "";
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 18);
		ImGui::InputTextWithHint("Filtrele", "Mod/alt mod/yazar adı...", nameFilterBuf, IM_ARRAYSIZE(nameFilterBuf));
		ImGui::SameLine();
		UICommon::HelpMarker("Listeyi filtrelemek için mod, alt mod veya yazar adının bir kısmını yazın.");

		const float offset = ImGui::CalcTextSize("İnceleme Modu").x + ImGui::CalcTextSize("Kullanıcı Modu").x + ImGui::CalcTextSize("Yazar Modu").x + ImGui::CalcTextSize("(?)").x + 100.f;
		ImGui::SameLine(ImGui::GetWindowWidth() - offset);

		ImGui::RadioButton("İnceleme Modu", reinterpret_cast<int*>(&_editMode), 0);
		ImGui::SameLine();
		ImGui::RadioButton("Kullanıcı Modu", reinterpret_cast<int*>(&_editMode), 1);
		ImGui::SameLine();
		ImGui::RadioButton("Yazar Modu", reinterpret_cast<int*>(&_editMode), 2);
		ImGui::SameLine();
		UICommon::HelpMarker("Yazar modunda düzenleme yapmak, mod klasörlerinde bulunan orijinal yapılandırma dosyalarını değiştirir. Kullanıcı modu ise, ayarlar yeniden yüklendiğinde orijinal dosyanın üzerine geçecek yeni bir yapılandırma dosyası oluşturur ve kaydeder. Bu işlem orijinal dosyayı etkilemez.");

		ImGui::Separator();

		if (ImGui::BeginChild("Mods")) {
			bool bFirstDisplayed = true;
			OpenAnimationReplacer::GetSingleton().ForEachSortedReplacerMod([&](ReplacerMod* a_replacerMod) {
				// Parse names for filtering and duplicate names
				std::unordered_map<std::string, SubModNameFilterResult> subModNameFilterResults{};
				const bool bEntireReplacerModMatchesFilter = !std::strlen(nameFilterBuf) || Utils::ContainsStringIgnoreCase(a_replacerMod->GetName(), nameFilterBuf) || Utils::ContainsStringIgnoreCase(a_replacerMod->GetAuthor(), nameFilterBuf);
				bool bDisplayMod = bEntireReplacerModMatchesFilter;

				a_replacerMod->ForEachSubMod([&](const SubMod* a_subMod) {
					SubModNameFilterResult res;
					const auto subModName = a_subMod->GetName();
					res.bDisplay = bEntireReplacerModMatchesFilter || Utils::ContainsStringIgnoreCase(subModName, nameFilterBuf);

					// if at least one submod matches filter, display the replacer mod
					if (res.bDisplay) {
						bDisplayMod = true;
					}

					auto [it, bInserted] = subModNameFilterResults.try_emplace(subModName.data(), res);
					if (!bInserted) {
						subModNameFilterResults[subModName.data()].bDuplicateName = true;
					}

					return RE::BSVisit::BSVisitControl::kContinue;
				});

				if (bDisplayMod) {
					if (!bFirstDisplayed && a_replacerMod->IsLegacy()) {
						ImGui::Separator();
					}
					DrawReplacerMod(a_replacerMod, subModNameFilterResults);
					bFirstDisplayed = false;
				}
			});
		}
		ImGui::EndChild();
	}

	void UIMain::DrawReplacerMod(ReplacerMod* a_replacerMod, std::unordered_map<std::string, SubModNameFilterResult>& a_filterResults)
	{
		if (!a_replacerMod) {
			return;
		}

		bool bNodeOpen = ImGui::TreeNodeEx(a_replacerMod, ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");

		if (a_replacerMod->IsDirty()) {
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::DIRTY_COLOR, "*");
		}

		if (a_replacerMod->GetConfigSource() == Parsing::ConfigSource::kUser) {
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::USER_MOD_COLOR, "(Kullanıcı)");
		}

		// node name
		ImGui::SameLine();
		if (a_replacerMod->HasInvalidConditions(false) || a_replacerMod->HasInvalidFunctions()) {
			UICommon::TextUnformattedColored(UICommon::INVALID_CONDITION_COLOR, a_replacerMod->GetName().data());
		} else {
			ImGui::TextUnformatted(a_replacerMod->GetName().data());
		}

		if (bNodeOpen) {
			// Mod name
			if (!a_replacerMod->IsLegacy() && _editMode == EditMode::kAuthor) {
				const std::string nameId = "Mod name##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + "name";
				ImGui::SetNextItemWidth(-150.f);
				std::string tempName(a_replacerMod->GetName());
				if (ImGui::InputTextWithHint(nameId.data(), "Mod adı", &tempName)) {
					a_replacerMod->SetName(tempName);
					a_replacerMod->SetDirty(true);
				}
			}

			// Mod author
			const std::string authorId = "Author##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + "author";
			if (!a_replacerMod->IsLegacy() && _editMode == EditMode::kAuthor) {
				ImGui::SetNextItemWidth(250.f);
				std::string tempAuthor(a_replacerMod->GetAuthor());
				if (ImGui::InputTextWithHint(authorId.data(), "Yazar", &tempAuthor)) {
					a_replacerMod->SetAuthor(tempAuthor);
					a_replacerMod->SetDirty(true);
				}
			} else if (!a_replacerMod->GetAuthor().empty()) {
				if (ImGui::BeginTable(authorId.data(), 1, ImGuiTableFlags_BordersOuter)) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					UICommon::TextDescriptionRightAligned("Yazar");
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted(a_replacerMod->GetAuthor().data());
					ImGui::EndTable();
				}
			}

			// Mod description
			const std::string descriptionId = "Mod description##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + "description";
			if (!a_replacerMod->IsLegacy() && _editMode == EditMode::kAuthor) {
				ImGui::SetNextItemWidth(-150.f);
				std::string tempDescription(a_replacerMod->GetDescription());
				if (ImGui::InputTextMultiline(descriptionId.data(), &tempDescription, ImVec2(0, ImGui::GetTextLineHeight() * 5))) {
					a_replacerMod->SetDescription(tempDescription);
					a_replacerMod->SetDirty(true);
				}
				ImGui::Spacing();
			} else if (!a_replacerMod->GetDescription().empty()) {
				if (ImGui::BeginTable(descriptionId.data(), 1, ImGuiTableFlags_BordersOuter)) {
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();
					UICommon::TextDescriptionRightAligned("Açıklama");
					UICommon::TextUnformattedWrapped(a_replacerMod->GetDescription().data());
					ImGui::EndTable();
				}
				ImGui::Spacing();
			}

			// Condition presets
			if (_editMode > EditMode::kNone || a_replacerMod->HasConditionPresets()) {
				const std::string conditionPresetsLabel = "Koşul ön ayarları##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + "conditionPresets";
				ImGuiTreeNodeFlags flags = a_replacerMod->HasConditionPresets() ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
				if (ImGui::CollapsingHeader(conditionPresetsLabel.data(), flags)) {
					ImGui::AlignTextToFramePadding();
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

					if (a_replacerMod->HasConditionPresets()) {
						bool bShouldSort = false;
						a_replacerMod->ForEachConditionPreset([&](Conditions::ConditionPreset* a_preset) {
							if (DrawConditionPreset(a_replacerMod, a_preset, bShouldSort)) {
								a_replacerMod->SetDirty(true);
							}

							return RE::BSVisit::BSVisitControl::kContinue;
						});

						if (bShouldSort) {
							a_replacerMod->SortConditionPresets();
						}
					}

					if (_editMode > EditMode::kNone) {
						// Add condition preset button
						constexpr auto popupName = "Yeni koşul ön ayarı ekleniyor"sv;
						if (ImGui::Button("Yeni koşul ön ayarı ekle")) {
							const auto popupPos = ImGui::GetCursorScreenPos();
							ImGui::SetNextWindowPos(popupPos);
							ImGui::OpenPopup(popupName.data());
						}

						if (ImGui::BeginPopupModal(popupName.data(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
							std::string conditionPresetName;
							if (ImGui::InputTextWithHint("##ConditionPresetName", "Benzersiz bir isim yazın...", &conditionPresetName, ImGuiInputTextFlags_EnterReturnsTrue)) {
								if (conditionPresetName.size() > 2 && !a_replacerMod->HasConditionPreset(conditionPresetName)) {
									auto newConditionPreset = std::make_unique<Conditions::ConditionPreset>(conditionPresetName, ""sv);
									a_replacerMod->AddConditionPreset(newConditionPreset);
									a_replacerMod->SetDirty(true);
									ImGui::CloseCurrentPopup();
								}
							}
							ImGui::SetItemDefaultFocus();
							ImGui::SameLine();
							if (ImGui::Button("İptal")) {
								ImGui::CloseCurrentPopup();
							}
							ImGui::EndPopup();
						}
					}

					ImGui::PopStyleVar();
				}
			}

			// Submods
			ImGui::TextUnformatted("Alt modlar:");
			a_replacerMod->ForEachSubMod([&](SubMod* a_subMod) {
				// Filter
				const auto search = a_filterResults.find(a_subMod->GetName().data());
				if (search != a_filterResults.end()) {
					const SubModNameFilterResult& filterResult = search->second;
					if (filterResult.bDisplay) {
						DrawSubMod(a_replacerMod, a_subMod, filterResult.bDuplicateName);
					}
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});

			// Save mod config
			if (!a_replacerMod->IsLegacy() && _editMode > EditMode::kNone) {
				const bool bIsDirty = a_replacerMod->IsDirty();
				if (!bIsDirty) {
					ImGui::BeginDisabled();
				}
				ImGui::BeginDisabled(a_replacerMod->HasInvalidConditions(true) || a_replacerMod->HasInvalidFunctions());
				if (ImGui::Button(_editMode == EditMode::kAuthor ? "Mod yapılandırmasını kaydet (Yazar)" : "Mod yapılandırmasını kaydet (Kullanıcı)")) {
					a_replacerMod->SaveConfig(_editMode);
				}
				ImGui::EndDisabled();
				if (!bIsDirty) {
					ImGui::EndDisabled();
				}

				// Reload mod config
				ImGui::SameLine();
				UICommon::ButtonWithConfirmationModal("Mod yapılandırmasını yeniden yükle", "Yapılandırmayı yeniden yüklemek istediğinizden emin misiniz?\nBu işlem geri alınamaz!\n\n"sv, [&]() {
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReloadReplacerModConfigJob>(a_replacerMod);
				});

				// delete user config
				const bool bUserConfigExists = Utils::DoesUserConfigExist(a_replacerMod->GetPath());
				if (!bUserConfigExists) {
					ImGui::BeginDisabled();
				}
				ImGui::SameLine();
				UICommon::ButtonWithConfirmationModal("Kullanıcı yapılandırmasını sil", "Kullanıcı yapılandırmasını silmek istediğinizden emin misiniz?\nBu işlem geri alınamaz!\n\n"sv, [&]() {
					Utils::DeleteUserConfig(a_replacerMod->GetPath());
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReloadReplacerModConfigJob>(a_replacerMod);
				});
				if (!bUserConfigExists) {
					ImGui::EndDisabled();
				}
			}

			ImGui::TreePop();
		}
	}

	void UIMain::DrawSubMod(ReplacerMod* a_replacerMod, SubMod* a_subMod, bool a_bAddPathToName /*= false*/)
	{
		bool bStyleVarPushed = false;
		if (a_subMod->IsDisabled()) {
			auto& style = ImGui::GetStyle();
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.DisabledAlpha);
			bStyleVarPushed = true;
		}

		bool bNodeOpen = ImGui::TreeNodeEx(a_subMod, ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");

		// Disable checkbox
		ImGui::SameLine();
		if (_editMode > EditMode::kNone) {
			std::string idString = std::format("{}##bDisabled", reinterpret_cast<uintptr_t>(a_subMod));
			ImGui::PushID(idString.data());
			bool bEnabled = !a_subMod->IsDisabled();
			if (ImGui::Checkbox("##disableSubMod", &bEnabled)) {
				a_subMod->SetDisabled(!bEnabled);
				OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, true);
				a_subMod->SetDirty(true);
			}
			UICommon::AddTooltip("İşaret kaldırılırsa alt mod devre dışı bırakılır ve içerdiği değiştirici animasyonların hiçbiri hesaba katılmaz.");
			ImGui::PopID();
		}

		if (a_subMod->IsDirty()) {
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::DIRTY_COLOR, "*");
		}

		switch (a_subMod->GetConfigSource()) {
		case Parsing::ConfigSource::kUser:
			ImGui::SameLine();
			UICommon::TextUnformattedColored(UICommon::USER_MOD_COLOR, "(Kullanıcı)");
			break;
		case Parsing::ConfigSource::kLegacy:
			ImGui::SameLine();
			UICommon::TextUnformattedDisabled("(Eski)");
			break;
		case Parsing::ConfigSource::kLegacyActorBase:
			ImGui::SameLine();
			UICommon::TextUnformattedDisabled("(Eski ActorBase)");
			break;
		}

		// node name
		ImGui::SameLine();
		if (a_subMod->HasInvalidConditions() || a_subMod->HasInvalidFunctions()) {
			UICommon::TextUnformattedColored(UICommon::INVALID_CONDITION_COLOR, a_subMod->GetName().data());
		} else {
			ImGui::TextUnformatted(a_subMod->GetName().data());
		}
		ImGui::SameLine();

		if (a_bAddPathToName) {
			UICommon::TextUnformattedDisabled(a_subMod->GetPath().data());
			ImGui::SameLine();
		}

		float cursorPosX = ImGui::GetCursorPosX();

		std::string priorityText = "Öncelik: " + std::to_string(a_subMod->GetPriority());
		UICommon::SecondColumn(_firstColumnWidthPercent);
		if (ImGui::GetCursorPosX() < cursorPosX) {
			// make sure we don't draw priority text over the name
			ImGui::SetCursorPosX(cursorPosX);
		}
		ImGui::TextUnformatted(priorityText.data());

		if (bStyleVarPushed) {
			ImGui::PopStyleVar();
		}

		if (bNodeOpen) {
			// Submod name
			{
				if (_editMode == EditMode::kAuthor) {
					std::string subModNameId = "Submod name##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "name";
					ImGui::SetNextItemWidth(-150.f);
					std::string tempName(a_subMod->GetName());
					if (ImGui::InputTextWithHint(subModNameId.data(), "Alt mod adı", &tempName)) {
						a_subMod->SetName(tempName);
						a_subMod->SetDirty(true);
					}
				}
			}

// Submod description
			{
				std::string subModDescriptionId = "Submod description##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "description";
				if (_editMode == EditMode::kAuthor) {
					ImGui::SetNextItemWidth(-150.f);
					std::string tempDescription(a_subMod->GetDescription());
					if (ImGui::InputTextMultiline(subModDescriptionId.data(), &tempDescription, ImVec2(0, ImGui::GetTextLineHeight() * 5))) {
						a_subMod->SetDescription(tempDescription);
						a_subMod->SetDirty(true);
					}
				} else if (!a_subMod->GetDescription().empty()) {
					if (ImGui::BeginTable(subModDescriptionId.data(), 1, ImGuiTableFlags_BordersOuter)) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						UICommon::TextDescriptionRightAligned("Açıklama");
						UICommon::TextUnformattedWrapped(a_subMod->GetDescription().data());
						ImGui::EndTable();
					}
				}
			}

			// Submod priority
			{
				std::string priorityLabel = "Priority##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "priority";
				if (_editMode != EditMode::kNone) {
					int32_t tempPriority = a_subMod->GetPriority();
					if (ImGui::InputInt(priorityLabel.data(), &tempPriority, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
						a_subMod->SetPriority(tempPriority);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, true);
						a_subMod->SetDirty(true);
					}
				} else {
					if (ImGui::BeginTable(priorityLabel.data(), 1, ImGuiTableFlags_BordersOuter)) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						UICommon::TextDescriptionRightAligned("Öncelik");
						ImGui::TextUnformatted(std::to_string(a_subMod->GetPriority()).data());
						ImGui::EndTable();
					}
				}
			}

			auto tableWidth = ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("(?)").x - ImGui::GetStyle().FramePadding.x * 2;

			// Submod override animations folder
			{
				std::string overrideAnimationsFolderLabel = "Override animations folder##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "overrideAnimationsFolder";
				std::string overrideAnimationsFolderTooltip = "Ayarlanırsa, bu alt mod animasyonları kendi klasörü yerine belirtilen klasörden (üst dizindeki) yükler.";
				if (_editMode != EditMode::kNone) {
					ImGui::SetNextItemWidth(-220.f);
					std::string tempOverrideAnimationsFolder(a_subMod->GetOverrideAnimationsFolder());
					if (ImGui::InputTextWithHint(overrideAnimationsFolderLabel.data(), "Geçersiz kılma animasyon klasörü", &tempOverrideAnimationsFolder)) {
						a_subMod->SetOverrideAnimationsFolder(tempOverrideAnimationsFolder);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(overrideAnimationsFolderTooltip.data());
				} else if (!a_subMod->GetOverrideAnimationsFolder().empty()) {
					if (ImGui::BeginTable(overrideAnimationsFolderLabel.data(), 1, ImGuiTableFlags_BordersOuter, ImVec2(tableWidth, 0))) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						UICommon::TextDescriptionRightAligned("Geçersiz kılma animasyon klasörü");
						ImGui::TextUnformatted(a_subMod->GetOverrideAnimationsFolder().data());
						ImGui::EndTable();
					}
					ImGui::SameLine();
					UICommon::HelpMarker(overrideAnimationsFolderTooltip.data());
				}
			}

			// Submod required project name
			{
				std::string requiredProjectNameId = "Required project name##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "requiredProjectName";
				std::string requiredProjectNameTooltip = "Ayarlanırsa, bu alt mod yalnızca belirtilen proje adı için yüklenecektir. Tüm projeler için yüklenmesini istiyorsanız boş bırakın.";
				if (_editMode != EditMode::kNone) {
					ImGui::SetNextItemWidth(-220.f);
					std::string tempRequiredProjectName(a_subMod->GetRequiredProjectName());
					if (ImGui::InputTextWithHint(requiredProjectNameId.data(), "Gerekli proje adı", &tempRequiredProjectName)) {
						a_subMod->SetRequiredProjectName(tempRequiredProjectName);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(requiredProjectNameTooltip.data());
				} else if (!a_subMod->GetRequiredProjectName().empty()) {
					if (ImGui::BeginTable(requiredProjectNameId.data(), 1, ImGuiTableFlags_BordersOuter, ImVec2(tableWidth, 0))) {
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::AlignTextToFramePadding();
						UICommon::TextDescriptionRightAligned("Gerekli proje adı");
						ImGui::TextUnformatted(a_subMod->GetRequiredProjectName().data());
						ImGui::EndTable();
					}
					ImGui::SameLine();
					UICommon::HelpMarker(requiredProjectNameTooltip.data());
				}
			}

			// Submod ignore no triggers flag
			{
				std::string noTriggersFlagLabel = "\"Açıklamaları Tetikleyicilere Dönüştürme\" bayrağını yok say##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "ignoreDontConvertAnnotationsToTriggersFlag";
				std::string noTriggersFlagTooltip = "İşaretlenirse, varsayılan oyundaki bazı animasyon klipslerinde ayarlanmış olan \"Açıklamaları tetikleyicilere dönüştürme\" animasyon klip bayrağı yok sayılır. Bu, animasyon dosyasının kendi içinde açıklama olarak bulunan animasyon tetikleyicilerinin (olaylarının) artık yok sayılmak yerine çalışacağı anlamına gelir.";

				if (_editMode != EditMode::kNone) {
					bool tempIgnoreNoTriggersFlag = a_subMod->IsIgnoringDontConvertAnnotationsToTriggersFlag();
					if (ImGui::Checkbox(noTriggersFlagLabel.data(), &tempIgnoreNoTriggersFlag)) {
						a_subMod->SetIgnoringDontConvertAnnotationsToTriggersFlag(tempIgnoreNoTriggersFlag);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(noTriggersFlagTooltip.data());
				} else if (a_subMod->IsIgnoringDontConvertAnnotationsToTriggersFlag()) {
					ImGui::BeginDisabled();
					bool tempIgnoreNoTriggersFlag = a_subMod->IsIgnoringDontConvertAnnotationsToTriggersFlag();
					ImGui::Checkbox(noTriggersFlagLabel.data(), &tempIgnoreNoTriggersFlag);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(noTriggersFlagTooltip.data());
				}
			}

			// Submod triggers from annotations only
			{
				std::string triggersFromAnnotationsOnlyLabel = "Yalnızca açıklamalardaki tetikleyicileri kullan##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "triggersFromAnnotationsOnly";
				std::string triggersFromAnnotationsOnlyTooltip = "İşaretlenirse, davranış (behavior) dosyalarının içindeki animasyon kliplerinde \"gömülü\" olan tetikleyiciler yok sayılır. Yalnızca animasyon dosyasındaki açıklamalardan (annotations) gelen olaylar çalıştırılır.\n\"Açıklamaları tetikleyicilere dönüştürme\" bayrağına hala uyulur, bu nedenle gerekirse yukarıdaki ayarı da etkinleştirdiğinizden emin olun.";

				if (_editMode != EditMode::kNone) {
					bool tempTriggersFromAnnotationsOnly = a_subMod->IsOnlyUsingTriggersFromAnnotations();
					if (ImGui::Checkbox(triggersFromAnnotationsOnlyLabel.data(), &tempTriggersFromAnnotationsOnly)) {
						a_subMod->SetOnlyUsingTriggersFromAnnotations(tempTriggersFromAnnotationsOnly);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(triggersFromAnnotationsOnlyTooltip.data());
				} else if (a_subMod->IsIgnoringDontConvertAnnotationsToTriggersFlag()) {
					ImGui::BeginDisabled();
					bool tempTriggersFromAnnotationsOnly = a_subMod->IsOnlyUsingTriggersFromAnnotations();
					ImGui::Checkbox(triggersFromAnnotationsOnlyLabel.data(), &tempTriggersFromAnnotationsOnly);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(triggersFromAnnotationsOnlyTooltip.data());
				}
			}

			// Submod interruptible
			{
				std::string interruptibleLabel = "Kesilebilir (Interruptible)##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "interruptible";
				std::string interruptibleTooltip = "İşaretlenirse, koşullar her karede (frame) tekrar kontrol edilir ve gerekirse klip başka bir kliple değiştirilir. En çok döngüsel (looping) animasyonlar için yararlıdır.";

				if (_editMode != EditMode::kNone) {
					bool tempInterruptible = a_subMod->IsInterruptible();
					if (ImGui::Checkbox(interruptibleLabel.data(), &tempInterruptible)) {
						a_subMod->SetInterruptible(tempInterruptible);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(interruptibleTooltip.data());
				} else if (a_subMod->IsInterruptible()) {
					ImGui::BeginDisabled();
					bool tempInterruptible = a_subMod->IsInterruptible();
					ImGui::Checkbox(interruptibleLabel.data(), &tempInterruptible);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(interruptibleTooltip.data());
				}
			}

			const auto drawBlendTimeOption = [this, a_subMod](CustomBlendType a_type, std::string_view a_boolLabel, std::string_view a_sliderLabel, std::string_view a_tooltip) {
				if (_editMode != EditMode::kNone) {
					bool tempHasCustomBlendTime = a_subMod->HasCustomBlendTime(a_type);
					if (ImGui::Checkbox(a_boolLabel.data(), &tempHasCustomBlendTime)) {
						a_subMod->ToggleCustomBlendTime(a_type, tempHasCustomBlendTime);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					ImGui::BeginDisabled(!a_subMod->HasCustomBlendTime(a_type));
					float tempBlendTime = a_subMod->GetCustomBlendTime(a_type);
					ImGui::SetNextItemWidth(200.f);
					if (ImGui::SliderFloat(a_sliderLabel.data(), &tempBlendTime, 0.f, 1.f, "%.2f s", ImGuiSliderFlags_AlwaysClamp)) {
						a_subMod->SetCustomBlendTime(a_type, tempBlendTime);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(a_tooltip.data());
				} else if (a_subMod->HasCustomBlendTime(a_type)) {
					ImGui::BeginDisabled();
					float tempBlendTime = a_subMod->GetCustomBlendTime(a_type);
					ImGui::SetNextItemWidth(200.f);
					ImGui::SliderFloat(a_sliderLabel.data(), &tempBlendTime, 0.f, 1.f, "%.2f s", ImGuiSliderFlags_AlwaysClamp);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(a_tooltip.data());
				}
			};

			// Submod custom blend time on interrupt
			if (a_subMod->IsInterruptible()) {
				const std::string hasCustomBlendTimeLabel = "##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "hasCustomBlendTimeOnInterrupt";
				const std::string blendTimeLabel = "Kesilmede özel geçiş süresi (blend time)##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "blendTimeOnInterrupt";
				const std::string blendTimeTooltip = "Animasyon kesildiğinde bu alt modun animasyonu ile yeni animasyon arasında özel bir geçiş süresi belirler.";
				drawBlendTimeOption(CustomBlendType::kInterrupt, hasCustomBlendTimeLabel, blendTimeLabel, blendTimeTooltip);
			}

			// Submod replace on loop
			{
				std::string replaceOnLoopLabel = "Döngüde değiştir (Replace on loop)##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "replaceOnLoop";
				std::string replaceOnLoopTooltip = "İşaretlenirse, animasyon döngüsünde koşullar yeniden değerlendirilir ve gerekirse klip başka bir klifle değiştirilir. Varsayılan olarak etkindir.";

				if (_editMode != EditMode::kNone) {
					bool tempReplaceOnLoop = a_subMod->IsReevaluatingOnLoop();
					if (ImGui::Checkbox(replaceOnLoopLabel.data(), &tempReplaceOnLoop)) {
						a_subMod->SetReevaluatingOnLoop(tempReplaceOnLoop);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(replaceOnLoopTooltip.data());
				} else if (!a_subMod->IsReevaluatingOnLoop()) {
					ImGui::BeginDisabled();
					bool tempReplaceOnLoop = a_subMod->IsReevaluatingOnLoop();
					ImGui::Checkbox(replaceOnLoopLabel.data(), &tempReplaceOnLoop);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(replaceOnLoopTooltip.data());
				}
			}

			// Submod custom blend time on loop
			if (a_subMod->IsReevaluatingOnLoop()) {
				const std::string hasCustomBlendTimeLabel = "##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "hasCustomBlendTimeOnLoop";
				const std::string blendTimeLabel = "Döngüde özel geçiş süresi (blend time)##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "blendTimeOnLoop";
				const std::string blendTimeTooltip = "Döngü sırasında değiştirme yapılırken bu alt modun animasyonu ile yeni animasyon arasında özel bir geçiş süresi belirler.";
				drawBlendTimeOption(CustomBlendType::kLoop, hasCustomBlendTimeLabel, blendTimeLabel, blendTimeTooltip);
			}

			// Submod replace on echo
			{
				std::string replaceOnEchoLabel = "Yankıda değiştir (Replace on echo)##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "replaceOnEcho";
				std::string replaceOnEchoTooltip = "İşaretlenirse, animasyon yankılanmasında (echo) koşullar yeniden değerlendirilir ve gerekirse klip başka bir klifle değiştirilir. Birkaç animasyondaki görsel sorunlar nedeniyle varsayılan olarak devre dışıdır, yalnızca gerçekten ihtiyaç duyan animasyonlarda etkinleştirilmelidir.";

				if (_editMode != EditMode::kNone) {
					bool tempReplaceOnEcho = a_subMod->IsReevaluatingOnEcho();
					if (ImGui::Checkbox(replaceOnEchoLabel.data(), &tempReplaceOnEcho)) {
						a_subMod->SetReevaluatingOnEcho(tempReplaceOnEcho);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
ImGui::SameLine();
					UICommon::HelpMarker(replaceOnEchoTooltip.data());
				} else if (a_subMod->IsReevaluatingOnEcho()) {
					ImGui::BeginDisabled();
					bool tempReplaceOnEcho = a_subMod->IsReevaluatingOnEcho();
					ImGui::Checkbox(replaceOnEchoLabel.data(), &tempReplaceOnEcho);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(replaceOnEchoTooltip.data());
				}
			}

			// Submod custom blend time on echo
			if (a_subMod->IsReevaluatingOnEcho()) {
				const std::string hasCustomBlendTimeLabel = "##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "hasCustomBlendTimeOnEcho";
				const std::string blendTimeLabel = "Yankıda özel geçiş süresi (blend time)##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "blendTimeOnEcho";
				const std::string blendTimeTooltip = "Yankıda (echo) değiştirme yapılırken bu alt modun animasyonu ile yeni animasyon arasında özel bir geçiş süresi belirler.";
				drawBlendTimeOption(CustomBlendType::kEcho, hasCustomBlendTimeLabel, blendTimeLabel, blendTimeTooltip);
			}

			// Submod run functions on loop
			{
				std::string runFunctionsOnLoopLabel = "Döngüde fonksiyonları çalıştır##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "runFunctionsOnLoop";
				std::string runFunctionsOnLoopTooltip = "İşaretlenirse, animasyon klip döngüye girdiğinde OnDeactivate/OnActivate fonksiyonları çalıştırılır. Varsayılan olarak etkindir.";

				if (_editMode != EditMode::kNone) {
					bool tempRunFunctionsOnLoop = a_subMod->IsRunningFunctionsOnLoop();
					if (ImGui::Checkbox(runFunctionsOnLoopLabel.data(), &tempRunFunctionsOnLoop)) {
						a_subMod->SetRunningFunctionsOnLoop(tempRunFunctionsOnLoop);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(runFunctionsOnLoopTooltip.data());
				} else if (!a_subMod->IsRunningFunctionsOnLoop()) {
					ImGui::BeginDisabled();
					bool tempRunFunctionsOnLoop = a_subMod->IsRunningFunctionsOnLoop();
					ImGui::Checkbox(runFunctionsOnLoopLabel.data(), &tempRunFunctionsOnLoop);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(runFunctionsOnLoopTooltip.data());
				}
			}

			// Submod run functions on echo
			{
				std::string runFunctionsOnEchoLabel = "Yankıda fonksiyonları çalıştır##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "runFunctionsOnEcho";
				std::string runFunctionsOnEchoTooltip = "İşaretlenirse, animasyon klip yankılanmasında (echo) OnDeactivate/OnActivate fonksiyonları çalıştırılır. Varsayılan olarak etkindir.";

				if (_editMode != EditMode::kNone) {
					bool tempRunFunctionsOnEcho = a_subMod->IsRunningFunctionsOnEcho();
					if (ImGui::Checkbox(runFunctionsOnEchoLabel.data(), &tempRunFunctionsOnEcho)) {
						a_subMod->SetRunningFunctionsOnEcho(tempRunFunctionsOnEcho);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
						a_subMod->SetDirty(true);
					}
					ImGui::SameLine();
					UICommon::HelpMarker(runFunctionsOnEchoTooltip.data());
				} else if (!a_subMod->IsRunningFunctionsOnEcho()) {
					ImGui::BeginDisabled();
					bool tempRunFunctionsOnEcho = a_subMod->IsRunningFunctionsOnEcho();
					ImGui::Checkbox(runFunctionsOnEchoLabel.data(), &tempRunFunctionsOnEcho);
					ImGui::EndDisabled();
					ImGui::SameLine();
					UICommon::HelpMarker(runFunctionsOnEchoTooltip.data());
				}
			}

			// Submod animations
			{
				// list animation files
				std::string filesTreeNodeLabel = "Animasyon Dosyaları##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "filesNode";
				if (ImGui::CollapsingHeader(filesTreeNodeLabel.data())) {
					ImGui::AlignTextToFramePadding();
					UICommon::TextUnformattedWrapped("Bu bölüm yalnızca alt modda bulunan tüm animasyon dosyalarını listeler. Aşağıdaki \"Değiştirme Animasyonları\" bölümü ise her davranış projesi (behavior project) için yüklü olan gerçek değiştirme animasyonlarını listeler ve yapılandırmaya olanak tanır.");

					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

					std::string filesTableId = std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "filesTable";
					if (ImGui::BeginTable(filesTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
						const std::filesystem::path submodFullPath = a_subMod->GetPath();
						a_subMod->ForEachReplacementAnimationFile([&](const ReplacementAnimationFile& a_file) {
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::AlignTextToFramePadding();

							const bool bHasVariants = a_file.variants.has_value();

							const std::filesystem::path fileFullPath = a_file.fullPath;
							const std::filesystem::path relativePath = fileFullPath.lexically_relative(submodFullPath);

							UICommon::TextUnformattedEllipsisShort(a_file.fullPath.data(), relativePath.string().data(), nullptr, ImGui::GetContentRegionAvail().x);
							if (bHasVariants) {
								for (const auto& variant : *a_file.variants) {
									ImGui::TableNextRow();
									ImGui::TableSetColumnIndex(0);
									ImGui::AlignTextToFramePadding();

									const std::filesystem::path variantFullPath = variant.fullPath;
									const std::filesystem::path variantRelativePath = variantFullPath.lexically_relative(submodFullPath);

									ImGui::Indent();
									UICommon::TextUnformattedEllipsisShort(variant.fullPath.data(), variantRelativePath.string().data(), nullptr, ImGui::GetContentRegionAvail().x);
									ImGui::Unindent();
								}
							}
						});
						ImGui::EndTable();
					}
					ImGui::PopStyleVar();
				}

				std::string animationsTreeNodeLabel = "Değiştirme Animasyonları##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "animationsNode";
				if (ImGui::CollapsingHeader(animationsTreeNodeLabel.data())) {
					UnloadedAnimationsWarning();

					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

					std::string animationsTableId = std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "animationsTable";
					if (ImGui::BeginTable(animationsTableId.data(), 1, ImGuiTableFlags_Borders)) {
						const std::filesystem::path submodFullPath = a_subMod->GetPath();
						a_subMod->ForEachReplacementAnimation([&](ReplacementAnimation* a_replacementAnimation) {
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::AlignTextToFramePadding();

							const bool bAnimationDisabled = a_replacementAnimation->IsDisabled();
							if (bAnimationDisabled) {
								auto& style = ImGui::GetStyle();
								ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.DisabledAlpha);
							}

							if (_editMode != EditMode::kNone) {
								const std::string idString = std::format("{}##bDisabled", reinterpret_cast<uintptr_t>(a_replacementAnimation));
								ImGui::PushID(idString.data());
								bool bEnabled = !a_replacementAnimation->GetDisabled();
								if (ImGui::Checkbox("##disableReplacementAnimation", &bEnabled)) {
									a_replacementAnimation->SetDisabled(!bEnabled);
									OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::UpdateSubModJob>(a_subMod, false);
									a_subMod->SetDirty(true);
								}
								UICommon::AddTooltip("İşaret kaldırılırsa, bu değiştirme animasyonu devre dışı bırakılır ve hesaba katılmaz.");
								ImGui::PopID();
								ImGui::SameLine();
							}

							const bool bHasVariants = a_replacementAnimation->HasVariants();

							const auto refrToEvaluate = UIManager::GetSingleton().GetRefrToEvaluate();
							const bool bCanPreview = CanPreviewAnimation(refrToEvaluate, a_replacementAnimation);
							bool bIsPreviewing = IsPreviewingAnimation(refrToEvaluate, a_replacementAnimation);

							UICommon::TextUnformattedDisabled(std::format("[{}]", a_replacementAnimation->GetProjectName()).data());
							ImGui::SameLine();

							float previewButtonWidth = 0.f;
							if (bCanPreview || bIsPreviewing) {
								previewButtonWidth = GetPreviewButtonsWidth(a_replacementAnimation, bIsPreviewing);
							}

							const std::filesystem::path fullPath = a_replacementAnimation->GetAnimPath();
							const std::filesystem::path relativePath = fullPath.lexically_relative(submodFullPath);

							UICommon::TextUnformattedEllipsisShort(a_replacementAnimation->GetAnimPath().data(), relativePath.string().data(), nullptr, ImGui::GetContentRegionAvail().x - previewButtonWidth);

							// preview button(s)
							if (!bHasVariants) {  // don't draw for variants. each variant gets its own button
								DrawPreviewButtons(refrToEvaluate, a_replacementAnimation, previewButtonWidth, bCanPreview, bIsPreviewing);
							}

							// variants
							if (bHasVariants) {
								auto& variants = a_replacementAnimation->GetVariants();
								auto variantScope = variants.GetVariantStateScope();
								std::string variantsTableId = std::to_string(reinterpret_cast<std::uintptr_t>(&variants)) + "variantsTable";
								if (variantScope > Conditions::StateDataScope::kLocal) {
									ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, UICommon::CONDITION_SHARED_STATE_BORDER_COLOR);
								}
								if (ImGui::BeginTable(variantsTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
									ImGui::TableNextRow();
									ImGui::TableSetColumnIndex(0);
									ImGui::AlignTextToFramePadding();

									auto variantMode = variants.GetVariantMode();
									if (!a_replacementAnimation->IsSynchronizedAnimation()) {
										// variant mode
										if (_editMode != EditMode::kNone) {
											const std::string label = "Varyant Modu##" + std::to_string(reinterpret_cast<std::uintptr_t>(&variants)) + "variantMode";
											const std::string current = variantMode == VariantMode::kRandom ? "Rastgele" : "Sıralı";
											int tempVariantMode = static_cast<int>(variantMode);
											ImGui::SetNextItemWidth(ImGui::GetFontSize() * 15);
											if (ImGui::SliderInt(label.data(), &tempVariantMode, 0, 1, current.data(), ImGuiSliderFlags_NoInput)) {
												variantMode = static_cast<VariantMode>(tempVariantMode);
												variants.SetVariantMode(variantMode);
												a_replacementAnimation->UpdateVariantCache();
												a_subMod->SetDirty(true);
												OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
											}
										} else {
											UICommon::TextUnformattedDisabled("Varyant Modu:");
											ImGui::SameLine();
											ImGui::TextUnformatted(variantMode == VariantMode::kRandom ? "Rastgele" : "Sıralı");
										}
									}

									// variant scope
									{
										auto getScopeName = [](const Conditions::StateDataScope a_scope) {
											switch (a_scope) {
											case Conditions::StateDataScope::kLocal:
												return "Yerel (Local)"sv;
											case Conditions::StateDataScope::kSubMod:
												return "Alt Mod (Submod)"sv;
											case Conditions::StateDataScope::kReplacerMod:
												return "Değiştirici Mod (Replacer mod)"sv;
											}
											return "GEÇERSİZ"sv;
										};

										auto getScopeTooltip = [](const Conditions::StateDataScope a_scope) {
											switch (a_scope) {
											case Conditions::StateDataScope::kLocal:
												return "Varyant verisi (rastgele değer, oynatılma geçmişi) her aktif animasyon klibi için özeldir."sv;
											case Conditions::StateDataScope::kSubMod:
												return "Varyant verisi (rastgele değer, isteğe bağlı oynatılma geçmişi), varyant kapsamı aynı değere ayarlandığı sürece alt moddaki tüm animasyon klipleri arasında paylaşılır.\n\nVeriler, daha dar kapsamlardaki tüm aktif klipler pasifleşene kadar canlı tutulur."sv;
											case Conditions::StateDataScope::kReplacerMod:
												return "Varyant verisi (rastgele değer, isteğe bağlı oynatılma geçmişi), varyant kapsamı aynı değere ayarlandığı sürece tüm değiştirici moddaki tüm animasyon klipleri arasında paylaşılır.\n\nVeriler, daha dar kapsamlardaki tüm aktif klipler pasifleşene kadar canlı tutulur."sv;
											}
											return "GEÇERSİZ"sv;
										};

										if (_editMode != EditMode::kNone) {
											const std::string variantScopeLabel = "Varyant durum kapsamı##" + std::to_string(reinterpret_cast<std::uintptr_t>(&variants));
											if (ImGui::BeginCombo(variantScopeLabel.data(), getScopeName(variantScope).data())) {
												for (Conditions::StateDataScope i = Conditions::StateDataScope::kLocal; i <= Conditions::StateDataScope::kReplacerMod; i = static_cast<Conditions::StateDataScope>(static_cast<int32_t>(i) << 1)) {
													const bool bIsCurrent = i == variantScope;
													if (ImGui::Selectable(getScopeName(i).data(), bIsCurrent)) {
														if (!bIsCurrent) {
															variants.SetVariantStateScope(i);
															a_subMod->SetDirty(true);
															OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
														}
													}
													if (bIsCurrent) {
														ImGui::SetItemDefaultFocus();
													}
													UICommon::AddTooltip(getScopeTooltip(i).data());
												}
												ImGui::EndCombo();
											}
										} else {
											const auto scopeText = std::format("Varyant durum kapsamı: {}", getScopeName(variantScope));
											ImGui::TextUnformatted(scopeText.data());
											UICommon::AddTooltip(getScopeTooltip(variantScope).data());
										}
									}

									// blend between variants
									{
										bool tempShouldBlend = variants.ShouldBlendBetweenVariants();
										const std::string shouldBlendLabel = "Varyantlar arasında geçiş yap (Blend)##" + std::to_string(reinterpret_cast<std::uintptr_t>(&variants));
										ImGui::BeginDisabled(_editMode == EditMode::kNone);
										if (ImGui::Checkbox(shouldBlendLabel.data(), &tempShouldBlend)) {
											variants.SetShouldBlendBetweenVariants(tempShouldBlend);
											a_subMod->SetDirty(true);
										}
										ImGui::EndDisabled();
										ImGui::SameLine();
										UICommon::HelpMarker("Devre dışı bırakılırsa, varyantlar döngü ve yankılanma (echo) sırasında birbirleri arasında herhangi bir yumuşak geçiş süresine (blend time) sahip olmaz.");
									}

// reset random on loop / echo
									{
										bool tempShouldResetRandom = variants.ShouldResetRandomOnLoopOrEcho();
										const std::string shouldResetRandomLabel = "Döngü veya ekoda rastgeleyi sıfırla##" + std::to_string(reinterpret_cast<std::uintptr_t>(&variants));
										ImGui::BeginDisabled(_editMode == EditMode::kNone);
										if (ImGui::Checkbox(shouldResetRandomLabel.data(), &tempShouldResetRandom)) {
											variants.SetShouldResetRandomOnLoopOrEcho(tempShouldResetRandom);
											a_subMod->SetDirty(true);
										}
										ImGui::EndDisabled();
										ImGui::SameLine();
										UICommon::HelpMarker("Etkinleştirilirse, rastgele bir varyant seçmek için kullanılan rastgele sayı her döngüde veya ekoda sıfırlanacaktır.");
									}

									// share played history
									{
										if (variants.GetVariantStateScope() > Conditions::StateDataScope::kLocal) {
											bool tempShouldSharePlayedHistory = variants.ShouldSharePlayedHistory();
											const std::string shouldSharePlayedHistoryLabel = "Oynatılan geçmişi paylaş##" + std::to_string(reinterpret_cast<std::uintptr_t>(&variants));
											ImGui::BeginDisabled(_editMode == EditMode::kNone);
											if (ImGui::Checkbox(shouldSharePlayedHistoryLabel.data(), &tempShouldSharePlayedHistory)) {
												variants.SetShouldSharePlayedHistory(tempShouldSharePlayedHistory);
												a_subMod->SetDirty(true);
												OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
											}
											ImGui::EndDisabled();
											ImGui::SameLine();
											UICommon::HelpMarker("Etkinleştirilirse, oynatılan geçmiş (\"Bir kez oynat\" ayarı için) aynı varyant durum kapsamına sahip tüm varyantlar arasında paylaşılacaktır.");
										}
									}

									// variant list
									int32_t index = 0;
									a_replacementAnimation->ForEachVariant([&](Variant& a_variant) {
										const bool bVariantDisabled = a_variant.IsDisabled();
										if (bVariantDisabled) {
											const auto& style = ImGui::GetStyle();
											ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.DisabledAlpha);
										}

										ImGui::TableNextRow();
										ImGui::TableSetColumnIndex(0);
										ImGui::AlignTextToFramePadding();

										ImGui::Indent();
										if (_editMode != EditMode::kNone) {
											ImGui::PushID(&a_variant);
											bool bEnabled = !a_variant.IsDisabled();
											if (ImGui::Checkbox("##disableVariant", &bEnabled)) {
												a_variant.SetDisabled(!bEnabled);
												a_replacementAnimation->UpdateVariantCache();
												a_subMod->SetDirty(true);
												OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
											}
											UICommon::AddTooltip("İşaretlenmezse, yedek animasyon varyantı devre dışı bırakılacak ve dikkate alınmayacaktır.");
											ImGui::SameLine();

											if (variantMode == VariantMode::kSequential || a_variant.ShouldPlayOnce()) {
												ImGui::BeginDisabled(index == 0);
												if (ImGui::ArrowButton("Varyantı yukarı taşı", ImGuiDir_Up)) {
													variants.SwapVariants(index, index - 1);
													a_replacementAnimation->UpdateVariantCache();
													a_subMod->SetDirty(true);
													OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
												}
												ImGui::EndDisabled();
												ImGui::SameLine();

												ImGui::BeginDisabled(index >= variants.GetVariantCount() - 1);
												if (ImGui::ArrowButton("Varyantı aşağı taşı", ImGuiDir_Down)) {
													variants.SwapVariants(index, index + 1);
													a_replacementAnimation->UpdateVariantCache();
													a_subMod->SetDirty(true);
													OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
												}
												ImGui::EndDisabled();
												ImGui::SameLine();
											} else if (variantMode == VariantMode::kRandom) {
												float tempWeight = a_variant.GetWeight();
												ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
												if (ImGui::InputFloat("Ağırlık", &tempWeight, .01f, 1.0f, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue)) {
													tempWeight = std::max(0.f, tempWeight);
													a_variant.SetWeight(tempWeight);
													a_replacementAnimation->UpdateVariantCache();
													a_subMod->SetDirty(true);
													OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
												}
												UICommon::AddTooltip("Ağırlıklı rastgele seçim için kullanılan bu varyantın ağırlığı (örneğin ağırlığı 2 olan bir varyantın seçilme olasılığı, ağırlığı 1 olan bir varyanta göre iki kat daha fazladır)");
												ImGui::SameLine();
											}

											bool tempPlayOnce = a_variant.ShouldPlayOnce();
											if (ImGui::Checkbox(variantMode == VariantMode::kRandom ? "Önce ve bir kez oynat" : "Bir kez oynat", &tempPlayOnce)) {
												a_variant.SetPlayOnce(tempPlayOnce);
												a_replacementAnimation->UpdateVariantCache();
												a_subMod->SetDirty(true);
												OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
											}
											UICommon::AddTooltip(variantMode == VariantMode::kRandom ? "Bununla işaretlenen varyantlar, diğer rastgele varyantlardan önce sırayla oynatılır ve bir süre hareketsizlikten sonra animasyon verileri sıfırlanana kadar tekrarlanmaz." : "İşaretlenirse, varyant bir süre hareketsizlikten sonra animasyon verileri sıfırlanana kadar yalnızca bir kez oynatılacaktır.");
											ImGui::SameLine();
											ImGui::PopID();
										} else {
											if (variantMode == VariantMode::kSequential || a_variant.ShouldPlayOnce()) {
												if (a_variant.ShouldPlayOnce()) {
													ImGui::TextUnformatted("[Bir kez oynat]");
													UICommon::AddTooltip("Varyant, bir süre hareketsizlikten sonra animasyon verileri sıfırlanana kadar yalnızca bir kez oynatılacaktır.");
													ImGui::SameLine();
												}
											} else if (variantMode == VariantMode::kRandom) {
												UICommon::TextUnformattedDisabled("Ağırlık:");
												ImGui::SameLine();
												ImGui::TextUnformatted(std::format("{}", a_variant.GetWeight()).data());
												UICommon::AddTooltip("Ağırlıklı rastgele seçim için kullanılan bu varyantın ağırlığı (örneğin ağırlığı 2 olan bir varyantın seçilme olasılığı, ağırlığı 1 olan bir varyanta göre iki kat daha fazladır)");
												ImGui::SameLine();
											}
										}

										UICommon::TextUnformattedDisabled("Dosya adı:");
										ImGui::SameLine();

										bIsPreviewing = IsPreviewingAnimation(refrToEvaluate, a_replacementAnimation, a_variant.GetIndex());

										float variantPreviewButtonWidth = 0.f;
										if (bCanPreview || bIsPreviewing) {
											variantPreviewButtonWidth = GetPreviewButtonsWidth(a_replacementAnimation, bIsPreviewing);
										}

										std::filesystem::path fullVariantPath = a_replacementAnimation->GetAnimPath();
										fullVariantPath /= a_variant.GetFilename();

										UICommon::TextUnformattedEllipsisShort(fullVariantPath.string().data(), a_variant.GetFilename().data(), nullptr, ImGui::GetContentRegionAvail().x - variantPreviewButtonWidth);

										// preview variant button
										DrawPreviewButtons(refrToEvaluate, a_replacementAnimation, variantPreviewButtonWidth, bCanPreview, bIsPreviewing, &a_variant);

										ImGui::Unindent();

										if (bVariantDisabled) {
											ImGui::PopStyleVar();
										}

										++index;

										return RE::BSVisit::BSVisitControl::kContinue;
									});
									ImGui::EndTable();
								}
								if (variantScope > Conditions::StateDataScope::kLocal) {
									ImGui::PopStyleColor();
								}
							}

							if (bAnimationDisabled) {
								ImGui::PopStyleVar();
							}
						});
						ImGui::EndTable();
					}

					ImGui::PopStyleVar();
				}
			}

			// Submod conditions
			std::string conditionsTreeNodeLabel = "Koşullar##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "conditionsNode";
			if (ImGui::CollapsingHeader(conditionsTreeNodeLabel.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

				ImGui::Indent();
				const ImGuiStyle& style = ImGui::GetStyle();
				ImVec2 pos = ImGui::GetCursorScreenPos();
				pos.x += style.FramePadding.x;
				pos.y += style.FramePadding.y;
				ImGui::PushID(a_subMod->GetConditionSet());
				DrawConditionSet(a_subMod->GetConditionSet(), a_subMod, _editMode, Conditions::ConditionType::kNormal, UIManager::GetSingleton().GetRefrToEvaluate(), true, pos);
				ImGui::PopID();
				ImGui::Unindent();

				ImGui::Spacing();
				ImGui::PopStyleVar();
			}

			// Submod paired conditions
			if (a_subMod->HasSynchronizedAnimations()) {
				std::string pairedConditionsTreeNodeLabel = "Eşleşmiş Koşullar##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "pairedConditionsNode";
				if (ImGui::CollapsingHeader(pairedConditionsTreeNodeLabel.data(), ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

					ImGui::Indent();
					const ImGuiStyle& style = ImGui::GetStyle();
					ImVec2 pos = ImGui::GetCursorScreenPos();
					pos.x += style.FramePadding.x;
					pos.y += style.FramePadding.y;
					ImGui::PushID(a_subMod->GetSynchronizedConditionSet());
					DrawConditionSet(a_subMod->GetSynchronizedConditionSet(), a_subMod, _editMode, Conditions::ConditionType::kNormal, UIManager::GetSingleton().GetRefrToEvaluate(), true, pos);
					ImGui::PopID();
					ImGui::Unindent();

					ImGui::Spacing();
					ImGui::PopStyleVar();
				}
			}

			// Submod functions
			auto drawFunctionSet = [&](Functions::FunctionSetType a_functionSetType) {
				std::string functionsTreeNodeLabel;
				std::string helpMarkerText;
				switch (a_functionSetType) {
				case Functions::FunctionSetType::kOnActivate:
					functionsTreeNodeLabel = "Aktifleştiğinde##";
					helpMarkerText = "Bu gruptaki işlevler, bu alt moddan bir animasyon başladığında çalışacaktır.";
					break;
				case Functions::FunctionSetType::kOnDeactivate:
					functionsTreeNodeLabel = "Devre Dışı Kaldığında##";
					helpMarkerText = "Bu gruptaki işlevler, bu alt moddan bir animasyon sona erdiğinde çalışacaktır.";
					break;
				case Functions::FunctionSetType::kOnTrigger:
					functionsTreeNodeLabel = "Tetiklendiğinde##";
					helpMarkerText = "Bu gruptaki işlevler, bu alt moddan bir animasyon oynatılırken belirli bir animasyon olayı çağrıldığında çalışacaktır.";
					break;
				}

				ImGuiTreeNodeFlags_ treeNodeFlags = a_subMod->GetFunctionSet(a_functionSetType) ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

				functionsTreeNodeLabel += std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "functionsNode";
				bool bIsExpanded = ImGui::CollapsingHeader(functionsTreeNodeLabel.data(), treeNodeFlags);
				ImGui::SameLine();
				UICommon::HelpMarker(helpMarkerText.data());
				if (bIsExpanded) {
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

					ImGui::Indent();
					const ImGuiStyle& style = ImGui::GetStyle();
					ImVec2 pos = ImGui::GetCursorScreenPos();
					pos.x += style.FramePadding.x;
					pos.y += style.FramePadding.y;
					ImGui::PushID(a_subMod->GetFunctionSet(a_functionSetType));
					DrawFunctionSet(a_subMod->GetFunctionSet(a_functionSetType), a_subMod, _editMode, a_functionSetType, UIManager::GetSingleton().GetRefrToEvaluate(), true, pos);
					ImGui::PopID();
					ImGui::Unindent();

					ImGui::Spacing();
					ImGui::PopStyleVar();
				}
			};

			ImGuiTreeNodeFlags_ treeNodeFlags = a_subMod->HasAnyFunctionSet() ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

			if (_editMode != EditMode::kNone || a_subMod->HasAnyFunctionSet()) {
				std::string allFunctionsTreeNodeLabel = "İşlevler##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + std::to_string(reinterpret_cast<std::uintptr_t>(a_subMod)) + "functionsNode";
				if (ImGui::CollapsingHeader(allFunctionsTreeNodeLabel.data(), treeNodeFlags)) {
					ImGui::Indent();
					if (_editMode != EditMode::kNone || a_subMod->HasFunctionSet(Functions::FunctionSetType::kOnActivate)) {
						drawFunctionSet(Functions::FunctionSetType::kOnActivate);
					}
					if (_editMode != EditMode::kNone || a_subMod->HasFunctionSet(Functions::FunctionSetType::kOnDeactivate)) {
						drawFunctionSet(Functions::FunctionSetType::kOnDeactivate);
					}
					if (_editMode != EditMode::kNone || a_subMod->HasFunctionSet(Functions::FunctionSetType::kOnTrigger)) {
						drawFunctionSet(Functions::FunctionSetType::kOnTrigger);
					}
					ImGui::Unindent();
				}
			}

			if (_editMode != EditMode::kNone) {
				if (_editMode == EditMode::kAuthor && a_replacerMod->IsLegacy()) {
					// Save migration config
					ImGui::BeginDisabled(a_subMod->HasInvalidConditions() || a_subMod->HasInvalidFunctions());
					UICommon::ButtonWithConfirmationModal("Taşıma için geliştirici alt mod yapılandırmasını kaydet", "Bu eski bir mod olduğu için bu geliştirici yapılandırması okunmayacaktır.\nİşlevsellik sadece kolaylık sağlamak amacıyla buradadır.\nModlarınızı yeni yapıya taşırken oluşan dosyayı uygun klasöre kopyalayabilirsiniz.\n\n"sv, [&]() {
						a_subMod->SaveConfig(_editMode, false);
					});
					ImGui::EndDisabled();
				} else {
					// Save submod config
					bool bIsDirty = a_subMod->IsDirty();
					if (!bIsDirty) {
						ImGui::BeginDisabled();
					}
					ImGui::BeginDisabled(a_subMod->HasInvalidConditions() || a_subMod->HasInvalidFunctions());
					if (ImGui::Button(_editMode == EditMode::kAuthor ? "Alt mod yapılandırmasını kaydet (Geliştirici)" : "Alt mod yapılandırmasını kaydet (Kullanıcı)")) {
						a_subMod->SaveConfig(_editMode);
					}
					ImGui::EndDisabled();
					if (!bIsDirty) {
						ImGui::EndDisabled();
					}
				}

// Reload submod config
				ImGui::SameLine();
				UICommon::ButtonWithConfirmationModal("Alt mod yapılandırmasını yeniden yükle", "Yapılandırmayı yeniden yüklemek istediğinizden emin misiniz?\nBu işlem geri alınamaz!\n\n"sv, [&]() {
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReloadSubModConfigJob>(a_subMod);
				});

				// delete user config
				const bool bUserConfigExists = Utils::DoesUserConfigExist(a_subMod->GetPath());
				if (!bUserConfigExists) {
					ImGui::BeginDisabled();
				}
				ImGui::SameLine();
				UICommon::ButtonWithConfirmationModal("Alt mod kullanıcı yapılandırmasını sil", "Kullanıcı yapılandırmasını silmek istediğinizden emin misiniz?\nBu işlem geri alınamaz!\n\n"sv, [&]() {
					Utils::DeleteUserConfig(a_subMod->GetPath());
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReloadSubModConfigJob>(a_subMod);
				});
				if (!bUserConfigExists) {
					ImGui::EndDisabled();
				}

				// copy json to clipboard button
				if (a_replacerMod->IsLegacy() && _editMode == EditMode::kAuthor) {
					ImGui::SameLine();
					ImGui::BeginDisabled(a_subMod->HasInvalidConditions() || a_subMod->HasInvalidFunctions());
					if (ImGui::Button("Alt mod yapılandırmasını panoya kopyala")) {
						ImGui::LogToClipboard();
						ImGui::LogText(a_subMod->SerializeToString().data());
						ImGui::LogFinish();
					}
					ImGui::EndDisabled();
				}
			}

			ImGui::Spacing();

			ImGui::TreePop();
		}
	}

	void UIMain::DrawReplacementAnimations()
	{
		static char animPathFilterBuf[32] = "";
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 18);
		ImGui::InputTextWithHint("Filtrele", "Animasyon adı...", animPathFilterBuf, IM_ARRAYSIZE(animPathFilterBuf));
		ImGui::SameLine();
		UICommon::HelpMarker("Değiştirilecek animasyonlar listesini filtrelemek için animasyon yolunun bir bölümünü yazın.");

		ImGui::Separator();

		UnloadedAnimationsWarning();

		ImGui::Spacing();

		if (ImGui::BeginChild("Datas")) {
			OpenAnimationReplacer::GetSingleton().ForEachReplacerProjectData([&](RE::hkbCharacterStringData* a_stringData, const auto& a_projectData) {
				const auto& name = a_stringData->name;

				ImGui::PushID(a_stringData);
				if (ImGui::TreeNode(name.data())) {
					auto animCount = a_stringData->animationNames.size();
					auto totalCount = a_projectData->projectDBData->bindings->bindings.size();
					const float animPercent = static_cast<float>(totalCount) / static_cast<float>(Settings::uAnimationLimit);
					const std::string animPercentStr = std::format("{} ({} + {}) / {}", totalCount, animCount, totalCount - animCount, Settings::uAnimationLimit);
					ImGui::ProgressBar(animPercent, ImVec2(0.f, 0.f), animPercentStr.data());

					auto& map = a_projectData->originalIndexToAnimationReplacementsMap;

					std::vector<AnimationReplacements*> sortedReplacements;
					sortedReplacements.reserve(map.size());

					for (auto& [index, animReplacements] : map) {
						// Filter
						if (std::strlen(animPathFilterBuf) && !Utils::ContainsStringIgnoreCase(animReplacements->GetOriginalPath(), animPathFilterBuf)) {
							continue;
						}

						auto it = std::lower_bound(sortedReplacements.begin(), sortedReplacements.end(), animReplacements, [](const auto& a_lhs, const auto& a_rhs) {
							return a_lhs->GetOriginalPath() < a_rhs->GetOriginalPath();
						});
						sortedReplacements.insert(it, animReplacements.get());
					}

					for (auto& animReplacements : sortedReplacements) {
						ImGui::PushID(animReplacements);
						if (ImGui::TreeNode(animReplacements->GetOriginalPath().data())) {
							ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
							if (ImGui::BeginTable(animReplacements->GetOriginalPath().data(), 1, ImGuiTableFlags_BordersOuter)) {
								animReplacements->ForEachReplacementAnimation([this](auto animation) { DrawReplacementAnimation(animation); });

								ImGui::EndTable();
							}
							ImGui::PopStyleVar();
							ImGui::TreePop();
						}
						ImGui::PopID();
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			});
		}
		ImGui::EndChild();
	}

	void UIMain::DrawReplacementAnimation(ReplacementAnimation* a_replacementAnimation)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		// Evaluate
		auto evalResult = ConditionEvaluateResult::kFailure;
		const auto refrToEvaluate = UIManager::GetSingleton().GetRefrToEvaluate();
		if (refrToEvaluate) {
			if (Utils::ConditionSetHasRandomResult(a_replacementAnimation->GetConditionSet())) {
				evalResult = ConditionEvaluateResult::kRandom;
			} else if (a_replacementAnimation->EvaluateConditions(refrToEvaluate, nullptr)) {
				evalResult = ConditionEvaluateResult::kSuccess;
			}
		}

		const std::string nodeName = std::format("Öncelik: {}##{}", std::to_string(a_replacementAnimation->GetPriority()), reinterpret_cast<uintptr_t>(a_replacementAnimation));

		const bool bAnimationDisabled = a_replacementAnimation->IsDisabled();
		if (bAnimationDisabled) {
			auto& style = ImGui::GetStyle();
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.DisabledAlpha);
		}

		ImGui::PushID(&a_replacementAnimation);
		const bool bNodeOpen = ImGui::TreeNodeEx(nodeName.data(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);

		if (const auto parentSubMod = a_replacementAnimation->GetParentSubMod()) {
			if (parentSubMod->IsFromLegacyConfig()) {
				ImGui::SameLine();
				UICommon::TextUnformattedDisabled("Eski");
			} else if (const auto parentMod = parentSubMod->GetParentMod()) {
				ImGui::SameLine();
				UICommon::TextUnformattedDisabled(parentMod->GetName().data());
				ImGui::SameLine();
				UICommon::TextUnformattedDisabled("-");
				ImGui::SameLine();
				UICommon::TextUnformattedDisabled(parentSubMod->GetName().data());
			}
		}

		// Evaluate success/failure indicator
		if (refrToEvaluate) {
			UICommon::DrawConditionEvaluateResult(evalResult);
		}

		if (bNodeOpen) {
			const bool bCanPreview = CanPreviewAnimation(refrToEvaluate, a_replacementAnimation);
			bool bIsPreviewing = IsPreviewingAnimation(refrToEvaluate, a_replacementAnimation);

			const auto animPath = a_replacementAnimation->GetAnimPath();

			float previewButtonWidth = 0.f;
			if (bCanPreview || bIsPreviewing) {
				previewButtonWidth = GetPreviewButtonsWidth(a_replacementAnimation, bIsPreviewing);
			}
			UICommon::TextUnformattedEllipsis(animPath.data(), nullptr, ImGui::GetContentRegionAvail().x - previewButtonWidth);

			// preview button
			DrawPreviewButtons(refrToEvaluate, a_replacementAnimation, previewButtonWidth, bCanPreview, bIsPreviewing);

			// draw variants
			if (a_replacementAnimation->HasVariants()) {
				ImGui::TextUnformatted("Varyasyonlar:");
				ImGui::Indent();
				a_replacementAnimation->ForEachVariant([&](Variant& a_variant) {
					const bool bVariantDisabled = a_variant.IsDisabled();
					if (bVariantDisabled) {
						auto& style = ImGui::GetStyle();
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.DisabledAlpha);
					}

					UICommon::TextUnformattedDisabled("Ağırlık:");
					ImGui::SameLine();
					ImGui::TextUnformatted(std::format("{}", a_variant.GetWeight()).data());
					UICommon::AddTooltip("Ağırlıklı rastgele seçim için kullanılan bu varyasyonun ağırlığı (ör. ağırlığı 2 olan bir varyasyonun seçilme olasılığı, ağırlığı 1 olana göre iki kat daha fazladır).");
					ImGui::SameLine();
					UICommon::TextUnformattedDisabled("Dosya adı:");

					bIsPreviewing = IsPreviewingAnimation(refrToEvaluate, a_replacementAnimation, a_variant.GetIndex());

					float variantPreviewButtonWidth = 0.f;
					if (bCanPreview || bIsPreviewing) {
						variantPreviewButtonWidth = GetPreviewButtonsWidth(a_replacementAnimation, bIsPreviewing);
					}
					ImGui::SameLine();
					UICommon::TextUnformattedEllipsis(a_variant.GetFilename().data(), nullptr, ImGui::GetContentRegionAvail().x - variantPreviewButtonWidth);

					// preview variant button
					DrawPreviewButtons(refrToEvaluate, a_replacementAnimation, variantPreviewButtonWidth, bCanPreview, bIsPreviewing, &a_variant);

					if (bVariantDisabled) {
						ImGui::PopStyleVar();
					}

					return RE::BSVisit::BSVisitControl::kContinue;
				});
				ImGui::Unindent();
			}
			DrawConditionSet(a_replacementAnimation->GetConditionSet(), a_replacementAnimation->GetParentSubMod(), EditMode::kNone, Conditions::ConditionType::kNormal, refrToEvaluate, true, ImGui::GetCursorScreenPos());
			ImGui::TreePop();
		}
		ImGui::PopID();

		if (bAnimationDisabled) {
			ImGui::PopStyleVar();
		}
	}

	bool UIMain::DrawConditionSet(Conditions::ConditionSet* a_conditionSet, SubMod* a_parentSubMod, EditMode a_editMode, Conditions::ConditionType a_conditionType, RE::TESObjectREFR* a_refrToEvaluate, bool a_bDrawLines, const ImVec2& a_drawStartPos)
	{
		if (!a_conditionSet) {
			return false;
		}

		//ImGui::TableNextRow();
		//ImGui::TableSetColumnIndex(0);

		bool bSetDirty = false;

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImGuiStyle& style = ImGui::GetStyle();

		ImVec2 vertLineStart = a_drawStartPos;
		vertLineStart.y += style.FramePadding.x;
		vertLineStart.x -= style.IndentSpacing * 0.6f;
		ImVec2 vertLineEnd = vertLineStart;

		const float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;

		if (!a_conditionSet->IsEmpty()) {
			a_conditionSet->ForEach([&](std::unique_ptr<Conditions::ICondition>& a_condition) {
				const ImRect nodeRect = DrawCondition(a_condition, a_conditionSet, a_parentSubMod, a_editMode, a_conditionType, a_refrToEvaluate, bSetDirty);
				if (a_bDrawLines) {
					const float midPoint = (nodeRect.Min.y + nodeRect.Max.y) / 2.f;
					constexpr float horLineLength = 10.f;
					drawList->AddLine(ImVec2(vertLineStart.x, midPoint), ImVec2(vertLineStart.x + horLineLength, midPoint), ImGui::GetColorU32(UICommon::TREE_LINE_COLOR));
					vertLineEnd.y = midPoint;
				}

				return RE::BSVisit::BSVisitControl::kContinue;
			});
		} else {
			DrawBlankCondition(a_conditionSet, a_editMode, a_conditionType);
		}

		if (a_bDrawLines) {
			drawList->AddLine(vertLineStart, vertLineEnd, ImGui::GetColorU32(UICommon::TREE_LINE_COLOR));
		}

		if (a_editMode > EditMode::kNone) {
			const bool bIsConditionPreset = a_conditionType == Conditions::ConditionType::kPreset;

			// Add condition button
			if (ImGui::Button("Yeni koşul ekle")) {
				if (bIsConditionPreset && _lastAddNewConditionName == "PRESET") {
					_lastAddNewConditionName.clear();
				}
				if (_lastAddNewConditionName.empty()) {
					auto isFormCondition = Conditions::CreateCondition("IsForm"sv);
					a_conditionSet->Add(isFormCondition, true);
					bSetDirty = true;
				} else {
					auto newCondition = Conditions::CreateCondition(_lastAddNewConditionName);
					a_conditionSet->Add(newCondition, true);
					bSetDirty = true;
				}
			}

			// Condition set functions button
			ImGui::SameLine(0.f, 20.f);
			const auto popupId = std::string("Koşul seti Fonksiyonları##") + std::to_string(reinterpret_cast<uintptr_t>(a_conditionSet));
			if (UICommon::PopupToggleButton("Koşul seti...", popupId.data())) {
				ImGui::OpenPopup(popupId.data());
			}

			if (ImGui::BeginPopupContextItem(popupId.data())) {
				const auto xButtonSize = ImGui::CalcTextSize("Koşul setini yapıştır").x + style.FramePadding.x * 2 + style.ItemSpacing.x;

				// Copy conditions button
				ImGui::BeginDisabled(a_conditionSet->IsEmpty() || !a_conditionSet->IsValid());
				if (ImGui::Button("Koşul setini kopyala", ImVec2(xButtonSize, 0))) {
					ImGui::CloseCurrentPopup();
					_conditionSetCopy = DuplicateConditionSet(a_conditionSet);
				}
				ImGui::EndDisabled();

				// Paste conditions button
				const bool bPasteEnabled = _conditionSetCopy && !(bIsConditionPreset && ConditionSetContainsPreset(_conditionSetCopy.get()));
				ImGui::BeginDisabled(!bPasteEnabled);
				if (ImGui::Button("Koşul setini yapıştır", ImVec2(xButtonSize, 0))) {
					ImGui::CloseCurrentPopup();
					const auto duplicatedSet = DuplicateConditionSet(_conditionSetCopy.get());
					a_conditionSet->Append(duplicatedSet.get());
					bSetDirty = true;
				}
				ImGui::EndDisabled();
				// Paste tooltip
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
					ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
					ImGui::BeginTooltip();
					DrawConditionSet(_conditionSetCopy.get(), nullptr, EditMode::kNone, a_conditionType, nullptr, false, a_drawStartPos);
					ImGui::EndTooltip();
				}

				// Clear conditions button
				ImGui::BeginDisabled(a_conditionSet->IsEmpty());
				UICommon::ButtonWithConfirmationModal(
					"Koşul setini sil"sv, "Belirlenen koşul setini silmek istediğinizden emin misiniz?\nBu işlem geri alınamaz!\n\n"sv, [&]() {
						ImGui::ClosePopupsExceptModals();
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ClearConditionSetJob>(a_conditionSet);
						bSetDirty = true;
					},
					ImVec2(xButtonSize, 0));
				ImGui::EndDisabled();
				ImGui::EndPopup();
			}
		}

		return bSetDirty;
	}

	bool UIMain::DrawFunctionSet(Functions::FunctionSet* a_functionSet, SubMod* a_parentSubMod, EditMode a_editMode, Functions::FunctionSetType a_functionSetType, RE::TESObjectREFR* a_refrToEvaluate, bool a_bDrawLines, const ImVec2& a_drawStartPos)
	{
		bool bSetDirty = false;

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImGuiStyle& style = ImGui::GetStyle();

		ImVec2 vertLineStart = a_drawStartPos;
		vertLineStart.y += style.FramePadding.x;
		vertLineStart.x -= style.IndentSpacing * 0.6f;
		ImVec2 vertLineEnd = vertLineStart;

		const float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;

		if (a_functionSet && !a_functionSet->IsEmpty()) {
			a_functionSet->ForEach([&](std::unique_ptr<Functions::IFunction>& a_function) {
				const ImRect nodeRect = DrawFunction(a_function, a_functionSet, a_parentSubMod, a_editMode, a_functionSetType, a_refrToEvaluate, bSetDirty);
				if (a_bDrawLines) {
					const float midPoint = (nodeRect.Min.y + nodeRect.Max.y) / 2.f;
					constexpr float horLineLength = 10.f;
					drawList->AddLine(ImVec2(vertLineStart.x, midPoint), ImVec2(vertLineStart.x + horLineLength, midPoint), ImGui::GetColorU32(UICommon::TREE_LINE_COLOR));
					vertLineEnd.y = midPoint;
				}

				return RE::BSVisit::BSVisitControl::kContinue;
			});
		} else {
			DrawBlankFunction(a_functionSet, a_parentSubMod, a_editMode, a_functionSetType);
		}

		if (a_bDrawLines) {
			drawList->AddLine(vertLineStart, vertLineEnd, ImGui::GetColorU32(UICommon::TREE_LINE_COLOR));
		}

if (a_editMode > EditMode::kNone) {
			// Add function button
			if (ImGui::Button("Yeni fonksiyon ekle")) {
				if (!a_functionSet) {
					a_functionSet = a_parentSubMod->CreateOrGetFunctionSet(a_functionSetType);
				}
				if (_lastAddNewFunctionName.empty()) {
					auto defaultFunction = Functions::CreateFunction("PlaySound"sv);
					a_functionSet->Add(defaultFunction, true);
					bSetDirty = true;
				} else {
					auto newFunction = Functions::CreateFunction(_lastAddNewFunctionName);
					a_functionSet->Add(newFunction, true);
					bSetDirty = true;
				}
			}

			// Function set functions button
			ImGui::SameLine(0.f, 20.f);
			const auto popupId = std::string("Fonksiyon seti fonksiyonları##") + std::to_string(reinterpret_cast<uintptr_t>(a_parentSubMod)) + std::to_string(static_cast<uint8_t>(a_functionSetType)) + std::to_string(reinterpret_cast<uintptr_t>(a_functionSet));
			if (UICommon::PopupToggleButton("Fonksiyon seti...", popupId.data())) {
				ImGui::OpenPopup(popupId.data());
			}

			if (ImGui::BeginPopupContextItem(popupId.data())) {
				const auto xButtonSize = ImGui::CalcTextSize("Fonksiyon setini yapıştır").x + style.FramePadding.x * 2 + style.ItemSpacing.x;

				// Copy functions button
				ImGui::BeginDisabled(!a_functionSet || a_functionSet->IsEmpty() || !a_functionSet->IsValid());
				if (ImGui::Button("Fonksiyon setini kopyala", ImVec2(xButtonSize, 0))) {
					ImGui::CloseCurrentPopup();
					_functionSetCopy = DuplicateFunctionSet(a_functionSet);
				}
				ImGui::EndDisabled();

				// Paste functions button
				const bool bPasteEnabled = _functionSetCopy != nullptr;
				ImGui::BeginDisabled(!bPasteEnabled);
				if (ImGui::Button("Fonksiyon setini yapıştır", ImVec2(xButtonSize, 0))) {
					ImGui::CloseCurrentPopup();
					const auto duplicatedSet = DuplicateFunctionSet(_functionSetCopy.get());
					if (!a_functionSet) {
						a_functionSet = a_parentSubMod->CreateOrGetFunctionSet(a_functionSetType);
					}
					a_functionSet->Append(duplicatedSet.get());
					bSetDirty = true;
				}
				ImGui::EndDisabled();
				// Paste tooltip
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
					ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
					ImGui::BeginTooltip();
					DrawFunctionSet(_functionSetCopy.get(), nullptr, EditMode::kNone, a_functionSetType, nullptr, false, a_drawStartPos);
					ImGui::EndTooltip();
				}

				// Clear functions button
				ImGui::BeginDisabled(!a_functionSet || a_functionSet->IsEmpty());
				UICommon::ButtonWithConfirmationModal(
					"Fonksiyon setini sil"sv, "Fonksiyon setini silmek istediğinizden emin misiniz?\nBu işlem geri alınamaz!\n\n"sv, [&]() {
						ImGui::ClosePopupsExceptModals();
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ClearFunctionSetJob>(a_functionSet);
						bSetDirty = true;
					},
					ImVec2(xButtonSize, 0));
				ImGui::EndDisabled();
				ImGui::EndPopup();
			}
		}

		return bSetDirty;
	}

	ImRect UIMain::DrawCondition(std::unique_ptr<Conditions::ICondition>& a_condition, Conditions::ConditionSet* a_conditionSet, SubMod* a_parentSubMod, EditMode a_editMode, Conditions::ConditionType a_conditionType, RE::TESObjectREFR* a_refrToEvaluate, bool& a_bOutSetDirty)
	{
		ImRect conditionRect;

		// Evaluate
		auto evalResult = ConditionEvaluateResult::kFailure;
		if (Utils::ConditionHasRandomResult(a_condition.get())) {
			evalResult = ConditionEvaluateResult::kRandom;
		} else if (a_refrToEvaluate) {
			bEvaluatingConditionsForUI = true;
			const bool bEvaluationResult = a_condition->Evaluate(a_refrToEvaluate, nullptr, a_parentSubMod);
			bEvaluatingConditionsForUI = false;
			if (bEvaluationResult) {
				evalResult = ConditionEvaluateResult::kSuccess;
			}
		}

		//ImGui::BeginGroup();
		ImRect nodeRect;

		std::string conditionTableId = std::format("{}conditionTable", reinterpret_cast<uintptr_t>(a_condition.get()));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
		const bool bIsConditionPreset = a_condition->GetConditionType() == Conditions::ConditionType::kPreset;
		const bool bHasSharedState = Utils::ConditionHasStateComponentWithSharedScope(a_condition.get());
		if (bIsConditionPreset) {
			ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, UICommon::CONDITION_PRESET_BORDER_COLOR);
		} else if (bHasSharedState) {
			ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, UICommon::CONDITION_SHARED_STATE_BORDER_COLOR);
		}

		if (ImGui::BeginTable(conditionTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			//ImGui::AlignTextToFramePadding();

			if (bIsConditionPreset || bHasSharedState) {
				ImGui::PopStyleColor();  // ImGuiCol_TableBorderStrong
			}

			const auto conditionName = a_condition->GetName();
			std::string nodeName = conditionName.data();
			if (a_condition->IsNegated()) {
				nodeName.insert(0, "NOT ");
			}

			bool bStyleVarPushed = false;
			if (a_condition->IsDisabled()) {
				auto& style = ImGui::GetStyle();
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.Alpha * style.DisabledAlpha);
				bStyleVarPushed = true;
			}

			float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;
			ImVec2 nodePos = ImGui::GetCursorScreenPos();

			// Open node
			bool bNodeOpen = false;
			if (a_editMode > EditMode::kNone || a_condition->GetNumComponents() > 0) {
				bNodeOpen = ImGui::TreeNodeEx(a_condition.get(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
			} else {
				bNodeOpen = UICommon::TreeNodeCollapsedLeaf(a_condition.get(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
				//bNodeOpen = ImGui::TreeNodeEx(a_condition.get(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
			}

			// Condition context menu
			if (a_editMode > EditMode::kNone) {
				if (ImGui::BeginPopupContextItem()) {
					// copy button
					auto& style = ImGui::GetStyle();
					auto xButtonSize = ImGui::CalcTextSize("Koşulu aşağıya yapıştırın").x + style.FramePadding.x * 2 + style.ItemSpacing.x;
					ImGui::BeginDisabled(!a_condition->IsValid());
					if (ImGui::Button("Koşulu kopyala", ImVec2(xButtonSize, 0))) {
						_conditionCopy = DuplicateCondition(a_condition);
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();

					// paste button
					const bool bPasteEnabled = _conditionCopy && !(a_conditionType == Conditions::ConditionType::kPreset && ConditionContainsPreset(_conditionCopy.get()));
					ImGui::BeginDisabled(!bPasteEnabled);
					if (ImGui::Button("Koşulu aşağıya yapıştırın", ImVec2(xButtonSize, 0))) {
						auto duplicate = DuplicateCondition(_conditionCopy);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::InsertConditionJob>(duplicate, a_conditionSet, a_condition);
						a_bOutSetDirty = true;
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();
					// paste tooltip
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
						ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
						ImGui::BeginTooltip();
						bool bDummy = false;
						DrawCondition(_conditionCopy, a_conditionSet, nullptr, EditMode::kNone, a_conditionType, nullptr, bDummy);
						ImGui::EndTooltip();
					}

					// comment button
					if (a_condition->GetConditionAPIVersion() >= Conditions::ConditionAPIVersion::V4) {
						ImGui::Spacing();

						if (ImGui::Button("Yorumu düzenle", ImVec2(xButtonSize, 0))) {
							_commentState.Set(a_condition.get(), a_conditionSet);
							ImGui::CloseCurrentPopup();
						}
					}

					ImGui::Spacing();

					// delete button
					UICommon::ButtonWithConfirmationModal(
						"Koşulu sil"sv, "Bu koşulu silmek istediğinizden emin misiniz?\nBu işlem geri alınamaz!\n\n"sv, [&]() {
							ImGui::ClosePopupsExceptModals();
							OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveConditionJob>(a_condition, a_conditionSet);
							a_bOutSetDirty = true;
						},
						ImVec2(xButtonSize, 0));

					ImGui::EndPopup();
				}
			}

			// Condition description tooltip
			DrawInfoTooltip(a_condition.get());

			nodeRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

			// Drag & Drop source
			if (a_editMode > EditMode::kNone && a_condition->IsValid()) {
				if (BeginDragDropSourceEx(ImGuiDragDropFlags_SourceNoHoldToOpenOthers, ImVec2(tooltipWidth, 0))) {
					DragConditionPayload payload(a_condition, a_conditionSet);
					ImGui::SetDragDropPayload("DND_CONDITION", &payload, sizeof(DragConditionPayload));
					bool bDummy = false;
					DrawCondition(a_condition, a_conditionSet, nullptr, EditMode::kNone, a_conditionType, nullptr, bDummy);
					ImGui::EndDragDropSource();
				}
			}

			// Drag & Drop target - tree node
			if (a_editMode > EditMode::kNone) {
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("DND_CONDITION", ImGuiDragDropFlags_AcceptPeekOnly)) {
						DragConditionPayload payload = *static_cast<DragConditionPayload*>(imguiPayload->Data);

						const ImGuiStyle& style = ImGui::GetStyle();
						if (!bNodeOpen) {
							// Draw our own preview of the drop because we want to draw a line either above or below the condition
							float midPoint = (nodeRect.Min.y + nodeRect.Max.y) / 2.f;
							const auto upperHalf = ImRect(nodeRect.Min.x, nodeRect.Min.y, nodeRect.Max.x, midPoint);
							const auto lowerHalf = ImRect(nodeRect.Min.x, midPoint, nodeRect.Max.x, nodeRect.Max.y);

							bool bInsertAfter = ImGui::IsMouseHoveringRect(lowerHalf.Min, lowerHalf.Max);

							ImDrawList* drawList = ImGui::GetWindowDrawList();
							auto lineY = bInsertAfter ? nodeRect.Max.y + style.ItemSpacing.y * 0.5f : nodeRect.Min.y - style.ItemSpacing.y * 0.5f;
							drawList->AddLine(ImVec2(nodeRect.Min.x, lineY), ImVec2(nodeRect.Max.x, lineY), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 3.f);

							if (imguiPayload->IsDelivery()) {
								OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveConditionJob>(payload.condition, payload.conditionSet, a_condition, a_conditionSet, bInsertAfter);
							}
						} else {
							// Draw our own preview of the drop because we want to draw a line above the condition if we're hovering over the upper half of the node. Ignore anything below, we have the invisible button for that
							float midPoint = (nodeRect.Min.y + nodeRect.Max.y) / 2.f;
							const auto upperHalf = ImRect(nodeRect.Min.x, nodeRect.Min.y, nodeRect.Max.x, midPoint);

							if (ImGui::IsMouseHoveringRect(upperHalf.Min, upperHalf.Max)) {
								ImDrawList* drawList = ImGui::GetWindowDrawList();
								drawList->AddLine(ImVec2(nodeRect.Min.x, nodeRect.Min.y - style.ItemSpacing.y * 0.5f), ImVec2(nodeRect.Max.x, nodeRect.Min.y - style.ItemSpacing.y * 0.5f), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 3.f);

								if (imguiPayload->IsDelivery()) {
									OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveConditionJob>(payload.condition, payload.conditionSet, a_condition, a_conditionSet, false);
								}
							}
						}
					}
					ImGui::EndDragDropTarget();
				}
			}

			// Disable checkbox
			ImGui::SameLine();
			if (a_editMode > EditMode::kNone) {
				std::string idString = std::format("{}##bDisabled", reinterpret_cast<uintptr_t>(a_condition.get()));
				ImGui::PushID(idString.data());
				bool bEnabled = !a_condition->IsDisabled();
				if (ImGui::Checkbox("##toggleCondition", &bEnabled)) {
					a_condition->SetDisabled(!bEnabled);
					a_conditionSet->SetDirty(true);
					a_bOutSetDirty = true;
				}
				UICommon::AddTooltip("Koşulu açar/kapatır");
				ImGui::PopID();
			}

			// Condition name
			ImGui::SameLine();
			if (a_condition->IsValid()) {
				auto requiredPluginName = a_condition->GetRequiredPluginName();
				if (!requiredPluginName.empty()) {
					UICommon::TextUnformattedColored(UICommon::CUSTOM_CONDITION_COLOR, nodeName.data());
				} else if (a_condition->GetConditionType() == Conditions::ConditionType::kPreset) {
					UICommon::TextUnformattedColored(UICommon::CONDITION_PRESET_COLOR, nodeName.data());
				} else {
					ImGui::TextUnformatted(nodeName.data());
				}
			} else {
				UICommon::TextUnformattedColored(UICommon::INVALID_CONDITION_COLOR, nodeName.data());
			}

			ImVec2 cursorPos = ImGui::GetCursorScreenPos();

			// Condition comment
			ImGui::SameLine();
			if (a_condition->GetConditionAPIVersion() >= Conditions::ConditionAPIVersion::V4) {
				if (!a_condition->GetComment().empty()) {
					std::string comment = a_condition->GetComment().c_str();
					float maxCommentWidth = (ImGui::GetWindowContentRegionMax().x * _firstColumnWidthPercent) - ImGui::GetCursorPos().x;
					UICommon::TextUnformattedEllipsisColored(UICommon::COMMENT_COLOR, std::format("{}", comment).data(), nullptr, maxCommentWidth);
				}
			}

			// Right column, argument text
			UICommon::SecondColumn(_firstColumnWidthPercent);
			const auto argument = a_condition->GetArgument();
			ImGui::TextUnformatted(argument.data());

			//ImGui::TableSetColumnIndex(0);

			// Evaluate success/failure indicator
			if (a_refrToEvaluate) {
				UICommon::DrawConditionEvaluateResult(evalResult);
			}

			conditionRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

			// Node contents
			if (bNodeOpen) {
				ImGui::Spacing();

				if (a_editMode > EditMode::kNone) {
					// negate checkbox
					bool bNOT = a_condition->IsNegated();
					if (ImGui::Checkbox("Tersine Çevir", &bNOT)) {
						a_condition->SetNegated(bNOT);
						a_conditionSet->SetDirty(true);
						a_bOutSetDirty = true;
					}
					UICommon::AddTooltip("Koşulu tersine çevirir");

					// select condition type
					ImGui::SameLine();
					const float conditionComboWidth = UICommon::FirstColumnWidth(_firstColumnWidthPercent);
					ImGui::SetNextItemWidth(conditionComboWidth);

					const auto& conditionInfos = _conditionComboFilter.GetConditionInfos(a_conditionType);
					if (conditionInfos.empty()) {
						_conditionComboFilter.CacheInfos();
					}

					int selectedItem = -1;
					const Info* currentConditionInfo = nullptr;

					auto it = std::ranges::find_if(conditionInfos, [&](const Info& a_conditionInfo) {
						return a_conditionInfo.name == std::string_view(conditionName);
					});

					if (it != conditionInfos.end()) {
						selectedItem = static_cast<int>(std::distance(conditionInfos.begin(), it));
						currentConditionInfo = &*it;
					}

if (_conditionComboFilter.ComboFilter("##Condition type", selectedItem, conditionInfos, currentConditionInfo, ImGuiComboFlags_HeightLarge, &UIMain::DrawInfoTooltip)) {
						if (selectedItem >= 0 && selectedItem < conditionInfos.size() && OpenAnimationReplacer::GetSingleton().HasConditionFactory(conditionInfos[selectedItem].name)) {
							_lastAddNewConditionName = conditionInfos[selectedItem].name;
							OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReplaceConditionJob>(a_condition, conditionInfos[selectedItem].name, a_conditionSet);
							a_bOutSetDirty = true;
						}
					}

					// remove condition button
					UICommon::SecondColumn(_firstColumnWidthPercent);

					UICommon::ButtonWithConfirmationModal("Koşulu sil"sv, "Koşulu kaldırmak istediğinizden emin misiniz?\nBu işlem geri alınamaz!\n\n"sv, [&]() {
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveConditionJob>(a_condition, a_conditionSet);
						a_bOutSetDirty = true;
					});

					// Essential state
					{
						if (a_condition->GetConditionType() == Conditions::ConditionType::kCustom && a_condition->GetConditionAPIVersion() >= Conditions::ConditionAPIVersion::V3) {
							static std::map<Conditions::EssentialState, std::string_view> enumMap = {
								{ Conditions::EssentialState::kEssential, "Zorunlu" },
								{ Conditions::EssentialState::kNonEssential_True, "Zorunlu Değil - Kabul et" },
								{ Conditions::EssentialState::kNonEssential_False, "Zorunlu Değil - Reddet" }
							};
							ImGui::SameLine();
							std::string idString = std::format("{}##essential", reinterpret_cast<uintptr_t>(a_condition.get()));
							ImGui::PushID(idString.data());

							Conditions::EssentialState currentValue = a_condition->GetEssential();
							std::string currentEnumName;
							if (const auto search = enumMap.find(currentValue); search != enumMap.end()) {
								currentEnumName = search->second;
							} else {
								currentEnumName = std::format("Bilinmeyen ({})", static_cast<uint8_t>(currentValue));
							}

							if (ImGui::BeginCombo(idString.data(), currentEnumName.data())) {
								for (auto& [enumValue, enumName] : enumMap) {
									const bool bIsCurrent = enumValue == currentValue;
									if (ImGui::Selectable(enumName.data(), bIsCurrent)) {
										if (!bIsCurrent) {
											currentValue = enumValue;
											a_condition->SetEssential(currentValue);
											a_conditionSet->SetDirty(true);
											a_bOutSetDirty = true;
										}
									}
									if (bIsCurrent) {
										ImGui::SetItemDefaultFocus();
									}
								}
								ImGui::EndCombo();
							}
							ImGui::PopID();
							UICommon::AddTooltip("Bu koşulu uygulayan bir eklentiye sahip olmayan kullanıcılara hata bildirimi yapılmayacak ve koşul, seçilen seçeneğe bağlı olarak doğru veya yanlış olarak değerlendirilecektir.");
						}
					}
				}

				if (auto numComponents = a_condition->GetNumComponents(); numComponents > 0) {
					bool bHasStateComponent = false;
					for (uint32_t i = 0; i < numComponents; i++) {
						auto component = a_condition->GetComponent(i);
						const bool bIsMultiConditionComponent = component->GetType() == Conditions::ConditionComponentType::kMulti;
						const bool bIsConditionPresetComponent = component->GetType() == Conditions::ConditionComponentType::kPreset;
						if (component->GetType() == Conditions::ConditionComponentType::kState) {
							bHasStateComponent = true;
						}
						if (bIsMultiConditionComponent || bIsConditionPresetComponent) {
							const auto multiConditionComponent = static_cast<Conditions::IMultiConditionComponent*>(component);
							// draw conditions
							auto conditionType = a_conditionType == Conditions::ConditionType::kPreset || bIsConditionPresetComponent ? Conditions::ConditionType::kPreset : Conditions::ConditionType::kNormal;
							if (DrawConditionSet(multiConditionComponent->GetConditions(), a_parentSubMod, bIsConditionPresetComponent ? EditMode::kNone : a_editMode, conditionType, multiConditionComponent->GetShouldDrawEvaluateResultForChildConditions() ? a_condition->GetRefrToEvaluate(a_refrToEvaluate) : nullptr, true, cursorPos)) {
								a_conditionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
							// display component
							if (component->DisplayInUI(a_editMode != EditMode::kNone, _firstColumnWidthPercent)) {
								a_conditionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
						} else {
							ImGui::Separator();
							// write component name aligned to the right
							const auto componentName = component->GetName();
							UICommon::TextDescriptionRightAligned(componentName.data());
							// show component description on mouseover
							const auto componentDescription = component->GetDescription();
							if (!componentDescription.empty()) {
								UICommon::AddTooltip(componentDescription.data());
							}
							// display component
							if (component->DisplayInUI(a_editMode != EditMode::kNone, _firstColumnWidthPercent)) {
								a_conditionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
						}
					}
					if (bHasStateComponent && a_bOutSetDirty) {
						OpenAnimationReplacer::GetSingleton().ClearAllConditionStateData();
					}
				}

				// Display current value, if applicable
				const auto current = a_condition->GetCurrent(a_refrToEvaluate);
				if (!current.empty()) {
					ImGui::Separator();
					UICommon::TextUnformattedDisabled("Mevcut:");
					ImGui::SameLine();
					ImGui::TextUnformatted(current.data());
				}

				ImGui::Spacing();

				ImGui::TreePop();
			}

			if (bStyleVarPushed) {
				ImGui::PopStyleVar();
			}

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();  // ImGuiStyleVar_CellPadding

		auto rectMax = ImGui::GetItemRectMax();

		auto width = ImGui::GetItemRectSize().x;
		auto groupEnd = ImGui::GetCursorPos();
		const ImGuiStyle& style = ImGui::GetStyle();
		ImVec2 invisibleButtonStart = groupEnd;
		invisibleButtonStart.y -= style.ItemSpacing.y;
		ImGui::SetCursorPos(invisibleButtonStart);
		std::string conditionInvisibleDragAreaId = std::format("{}conditionInvisibleDragArea", reinterpret_cast<uintptr_t>(a_condition.get()));
		ImGui::InvisibleButton(conditionInvisibleDragAreaId.data(), ImVec2(width, style.ItemSpacing.y));

		// Drag & Drop target - invisible button
		if (a_editMode > EditMode::kNone) {
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("DND_CONDITION", ImGuiDragDropFlags_AcceptPeekOnly)) {
					DragConditionPayload payload = *static_cast<DragConditionPayload*>(imguiPayload->Data);
					// Draw our own preview of the drop because we want to draw a line below the condition

					ImDrawList* drawList = ImGui::GetWindowDrawList();
					drawList->AddLine(ImVec2(nodeRect.Min.x, rectMax.y + style.ItemSpacing.y * 0.5f), ImVec2(nodeRect.Max.x, rectMax.y + style.ItemSpacing.y * 0.5f), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 3.f);

					if (imguiPayload->IsDelivery()) {
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveConditionJob>(payload.condition, payload.conditionSet, a_condition, a_conditionSet, true);
					}
				}
				ImGui::EndDragDropTarget();
			}
		}

		ImGui::SetCursorPos(groupEnd);

		//ImGui::EndGroup();

		return conditionRect;
	}

	ImRect UIMain::DrawFunction(std::unique_ptr<Functions::IFunction>& a_function, Functions::FunctionSet* a_functionSet, SubMod* a_parentSubMod, EditMode a_editMode, Functions::FunctionSetType a_functionSetType, RE::TESObjectREFR* a_refrToEvaluate, bool& a_bOutSetDirty)
	{
		ImRect functionRect;

		ImRect nodeRect;

		std::string functionTableId = std::format("{}functionTable", reinterpret_cast<uintptr_t>(a_function.get()));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));

		if (ImGui::BeginTable(functionTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			//ImGui::AlignTextToFramePadding();

			const auto functionName = a_function->GetName();
			std::string nodeName = functionName.data();

			bool bStyleVarPushed = false;
			if (a_function->IsDisabled()) {
				auto& style = ImGui::GetStyle();
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, style.Alpha * style.DisabledAlpha);
				bStyleVarPushed = true;
			}

			float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;
			ImVec2 nodePos = ImGui::GetCursorScreenPos();

			// Open node
			bool bNodeOpen = false;
			if (a_editMode > EditMode::kNone || a_function->GetNumComponents() > 0) {
				bNodeOpen = ImGui::TreeNodeEx(a_function.get(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
			} else {
				bNodeOpen = UICommon::TreeNodeCollapsedLeaf(a_function.get(), ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
				//bNodeOpen = ImGui::TreeNodeEx(a_condition.get(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");
			}

			// Function context menu
			if (a_editMode > EditMode::kNone) {
				if (ImGui::BeginPopupContextItem()) {
					// copy button
					auto& style = ImGui::GetStyle();
					auto xButtonSize = ImGui::CalcTextSize("Fonksiyonu aşağıya yapıştırın").x + style.FramePadding.x * 2 + style.ItemSpacing.x;
					ImGui::BeginDisabled(!a_function->IsValid());
					if (ImGui::Button("Fonksiyonu kopyala", ImVec2(xButtonSize, 0))) {
						_functionCopy = DuplicateFunction(a_function);
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();

					// paste button
					const bool bPasteEnabled = _functionCopy != nullptr;
					ImGui::BeginDisabled(!bPasteEnabled);
					if (ImGui::Button("Fonksiyonu aşağıya yapıştırın", ImVec2(xButtonSize, 0))) {
						auto duplicate = DuplicateFunction(_functionCopy);
						if (!a_functionSet) {
							a_functionSet = a_parentSubMod->CreateOrGetFunctionSet(a_functionSetType);
						}
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::InsertFunctionJob>(duplicate, a_functionSet, a_function);
						a_bOutSetDirty = true;
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();

					// paste tooltip
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
						ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
						ImGui::BeginTooltip();
						bool bDummy = false;
						DrawFunction(_functionCopy, a_functionSet, nullptr, EditMode::kNone, a_functionSetType, nullptr, bDummy);
						ImGui::EndTooltip();
					}

					// comment button
					if (a_function->GetFunctionAPIVersion() >= Functions::FunctionAPIVersion::V2) {
						ImGui::Spacing();

						if (ImGui::Button("Yorumu düzenle", ImVec2(xButtonSize, 0))) {
							_commentState.Set(a_function.get(), a_functionSet);
							ImGui::CloseCurrentPopup();
						}
					}

					ImGui::Spacing();

					// delete button
					UICommon::ButtonWithConfirmationModal(
						"Fonksiyonu sil"sv, "Fonksiyonu silmek istediğinizden emin misiniz?\nBu işlem geri alınamaz!\n\n"sv, [&]() {
							ImGui::ClosePopupsExceptModals();
							OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveFunctionJob>(a_function, a_functionSet);
							a_bOutSetDirty = true;
						},
						ImVec2(xButtonSize, 0));

					ImGui::EndPopup();
				}
			}

			// Function description tooltip
			DrawInfoTooltip(a_function.get());

			nodeRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

			// Drag & Drop source
			if (a_editMode > EditMode::kNone && a_function->IsValid()) {
				if (BeginDragDropSourceEx(ImGuiDragDropFlags_SourceNoHoldToOpenOthers, ImVec2(tooltipWidth, 0))) {
					DragFunctionPayload payload(a_function, a_functionSet);
					ImGui::SetDragDropPayload("DND_FUNCTION", &payload, sizeof(DragConditionPayload));
					bool bDummy = false;
					DrawFunction(a_function, a_functionSet, nullptr, EditMode::kNone, a_functionSetType, nullptr, bDummy);
					ImGui::EndDragDropSource();
				}
			}

			// Drag & Drop target - tree node
			if (a_editMode > EditMode::kNone) {
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("DND_FUNCTION", ImGuiDragDropFlags_AcceptPeekOnly)) {
						DragFunctionPayload payload = *static_cast<DragFunctionPayload*>(imguiPayload->Data);

						const ImGuiStyle& style = ImGui::GetStyle();
						if (!bNodeOpen) {
							// Draw our own preview of the drop because we want to draw a line either above or below the function
							float midPoint = (nodeRect.Min.y + nodeRect.Max.y) / 2.f;
							const auto upperHalf = ImRect(nodeRect.Min.x, nodeRect.Min.y, nodeRect.Max.x, midPoint);
							const auto lowerHalf = ImRect(nodeRect.Min.x, midPoint, nodeRect.Max.x, nodeRect.Max.y);

							bool bInsertAfter = ImGui::IsMouseHoveringRect(lowerHalf.Min, lowerHalf.Max);

							ImDrawList* drawList = ImGui::GetWindowDrawList();
							auto lineY = bInsertAfter ? nodeRect.Max.y + style.ItemSpacing.y * 0.5f : nodeRect.Min.y - style.ItemSpacing.y * 0.5f;
							drawList->AddLine(ImVec2(nodeRect.Min.x, lineY), ImVec2(nodeRect.Max.x, lineY), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 3.f);

							if (imguiPayload->IsDelivery()) {
								OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveFunctionJob>(payload.function, payload.functionSet, a_function, a_functionSet, bInsertAfter);
							}
						} else {
							// Draw our own preview of the drop because we want to draw a line above the function if we're hovering over the upper half of the node. Ignore anything below, we have the invisible button for that
							float midPoint = (nodeRect.Min.y + nodeRect.Max.y) / 2.f;
							const auto upperHalf = ImRect(nodeRect.Min.x, nodeRect.Min.y, nodeRect.Max.x, midPoint);

							if (ImGui::IsMouseHoveringRect(upperHalf.Min, upperHalf.Max)) {
								ImDrawList* drawList = ImGui::GetWindowDrawList();
								drawList->AddLine(ImVec2(nodeRect.Min.x, nodeRect.Min.y - style.ItemSpacing.y * 0.5f), ImVec2(nodeRect.Max.x, nodeRect.Min.y - style.ItemSpacing.y * 0.5f), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 3.f);

								if (imguiPayload->IsDelivery()) {
									OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveFunctionJob>(payload.function, payload.functionSet, a_function, a_functionSet, false);
								}
							}
						}
					}
					ImGui::EndDragDropTarget();
				}
			}

// Disable checkbox
			ImGui::SameLine();
			if (a_editMode > EditMode::kNone) {
				std::string idString = std::format("{}##bDisabled", reinterpret_cast<uintptr_t>(a_function.get()));
				ImGui::PushID(idString.data());
				bool bEnabled = !a_function->IsDisabled();
				if (ImGui::Checkbox("##toggleFunction", &bEnabled)) {
					a_function->SetDisabled(!bEnabled);
					a_functionSet->SetDirty(true);
					a_bOutSetDirty = true;
				}
				UICommon::AddTooltip("Fonksiyonu açar/kapatır.");
				ImGui::PopID();
			}

			// Function name
			ImGui::SameLine();
			if (a_function->IsValid()) {
				auto requiredPluginName = a_function->GetRequiredPluginName();
				if (!requiredPluginName.empty()) {
					UICommon::TextUnformattedColored(UICommon::CUSTOM_CONDITION_COLOR, nodeName.data());
				} else {
					ImGui::TextUnformatted(nodeName.data());
				}
			} else {
				UICommon::TextUnformattedColored(UICommon::INVALID_CONDITION_COLOR, nodeName.data());
			}

			ImVec2 cursorPos = ImGui::GetCursorScreenPos();

			// Function comment
			ImGui::SameLine();
			if (a_function->GetFunctionAPIVersion() >= Functions::FunctionAPIVersion::V2) {
				if (!a_function->GetComment().empty()) {
					std::string comment = a_function->GetComment().c_str();
					float maxCommentWidth = (ImGui::GetWindowContentRegionMax().x * _firstColumnWidthPercent) - ImGui::GetCursorPos().x;
					UICommon::TextUnformattedEllipsisColored(UICommon::COMMENT_COLOR, std::format("{}", comment).data(), nullptr, maxCommentWidth);
				}
			}

			// Right column, argument text
			UICommon::SecondColumn(_firstColumnWidthPercent);
			const auto argument = a_function->GetArgument();
			ImGui::TextUnformatted(argument.data());

			//ImGui::TableSetColumnIndex(0);

			functionRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

			// Node contents
			if (bNodeOpen) {
				ImGui::Spacing();

				if (a_editMode > EditMode::kNone) {
					// select function type
					//ImGui::SameLine();
					const float functionComboWidth = UICommon::FirstColumnWidth(_firstColumnWidthPercent);
					ImGui::SetNextItemWidth(functionComboWidth);

					const auto& functionInfos = _functionComboFilter.GetFunctionInfos();
					if (functionInfos.empty()) {
						_functionComboFilter.CacheInfos();
					}

					int selectedItem = -1;
					const Info* currentFunctionInfo = nullptr;

					auto it = std::ranges::find_if(functionInfos, [&](const Info& a_functionInfo) {
						return a_functionInfo.name == std::string_view(functionName);
					});

					if (it != functionInfos.end()) {
						selectedItem = static_cast<int>(std::distance(functionInfos.begin(), it));
						currentFunctionInfo = &*it;
					}

					if (_functionComboFilter.ComboFilter("##Function type", selectedItem, functionInfos, currentFunctionInfo, ImGuiComboFlags_HeightLarge, &UIMain::DrawInfoTooltip)) {
						if (selectedItem >= 0 && selectedItem < functionInfos.size() && OpenAnimationReplacer::GetSingleton().HasFunctionFactory(functionInfos[selectedItem].name)) {
							_lastAddNewFunctionName = functionInfos[selectedItem].name;
							OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::ReplaceFunctionJob>(a_function, functionInfos[selectedItem].name, a_functionSet);
							a_bOutSetDirty = true;
						}
					}

					// remove function button
					UICommon::SecondColumn(_firstColumnWidthPercent);

					UICommon::ButtonWithConfirmationModal("Fonksiyonu sil"sv, "Bu fonksiyonu silmek istediğinizden emin misiniz?\nBu işlem geri alınamaz!\n\n"sv, [&]() {
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveFunctionJob>(a_function, a_functionSet);
						a_bOutSetDirty = true;
					});

					// Essential state
					{
						if (a_function->GetFunctionType() == Functions::FunctionType::kCustom) {
							static std::map<Functions::EssentialState, std::string_view> enumMap = {
								{ Functions::EssentialState::kEssential, "Zorunlu" },
								{ Functions::EssentialState::kNonEssential_True, "Zorunlu Değil - Kabul Et" },
								{ Functions::EssentialState::kNonEssential_False, "Zorunlu Değil - Reddet" }
							};
							ImGui::SameLine();
							std::string idString = std::format("{}##essential", reinterpret_cast<uintptr_t>(a_function.get()));
							ImGui::PushID(idString.data());

							Functions::EssentialState currentValue = a_function->GetEssential();
							std::string currentEnumName;
							if (const auto search = enumMap.find(currentValue); search != enumMap.end()) {
								currentEnumName = search->second;
							} else {
								currentEnumName = std::format("Bilinmeyen ({})", static_cast<uint8_t>(currentValue));
							}

							if (ImGui::BeginCombo(idString.data(), currentEnumName.data())) {
								for (auto& [enumValue, enumName] : enumMap) {
									const bool bIsCurrent = enumValue == currentValue;
									if (ImGui::Selectable(enumName.data(), bIsCurrent)) {
										if (!bIsCurrent) {
											currentValue = enumValue;
											a_function->SetEssential(currentValue);
											a_functionSet->SetDirty(true);
											a_bOutSetDirty = true;
										}
									}
									if (bIsCurrent) {
										ImGui::SetItemDefaultFocus();
									}
								}
								ImGui::EndCombo();
							}
							ImGui::PopID();
							UICommon::AddTooltip("Bu fonksiyonu uygulayan bir eklentiye sahip olmayan kullanıcılara hata bildirimi yapılmayacak ve fonksiyonun dönüş değeri seçilen seçeneğe bağlı olarak doğru veya yanlış olacaktır.");
						}
					}
				}

				if (auto numComponents = a_function->GetNumComponents(); numComponents > 0) {
					for (uint32_t i = 0; i < numComponents; i++) {
						auto component = a_function->GetComponent(i);
						const bool bIsMultiFunctionComponent = component->GetType() == Functions::FunctionComponentType::kMulti;
						const bool bIsConditionComponent = component->GetType() == Functions::FunctionComponentType::kCondition;
						if (bIsMultiFunctionComponent) {
							const auto multiFunctionComponent = static_cast<Functions::IMultiFunctionComponent*>(component);
							// draw functions
							if (DrawFunctionSet(multiFunctionComponent->GetFunctions(), a_parentSubMod, a_editMode, Functions::FunctionSetType::kNone, a_refrToEvaluate, true, cursorPos)) {
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
							// display component
							if (component->DisplayInUI(a_editMode != EditMode::kNone, _firstColumnWidthPercent)) {
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
						} else if (bIsConditionComponent) {
							const auto conditionFunctionComponent = static_cast<Functions::IConditionFunctionComponent*>(component);
							// draw conditions
							if (DrawConditionSet(conditionFunctionComponent->GetConditions(), a_parentSubMod, a_editMode, Conditions::ConditionType::kNormal, a_refrToEvaluate, true, cursorPos)) {
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
							ImGui::Separator();
							// draw functions
							if (DrawFunctionSet(conditionFunctionComponent->GetFunctions(), a_parentSubMod, a_editMode, Functions::FunctionSetType::kNone, a_refrToEvaluate, true, cursorPos)) {
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
							// display component
							if (component->DisplayInUI(a_editMode != EditMode::kNone, _firstColumnWidthPercent)) {
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
						} else {
							ImGui::Spacing();
							// write component name aligned to the right
							const auto componentName = component->GetName();
							UICommon::TextDescriptionRightAligned(componentName.data());
							// show component description on mouseover
							const auto componentDescription = component->GetDescription();
							if (!componentDescription.empty()) {
								UICommon::AddTooltip(componentDescription.data());
							}
							// display component
							if (component->DisplayInUI(a_editMode != EditMode::kNone, _firstColumnWidthPercent)) {
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
						}
					}
				}

				// triggers
				if (a_functionSetType == Functions::FunctionSetType::kOnTrigger) {
					std::string triggersLabel = std::format("Tetikleyiciler##{}", reinterpret_cast<uintptr_t>(a_function.get()));
					bool bIsOpen = ImGui::CollapsingHeader(triggersLabel.data(), ImGuiTreeNodeFlags_DefaultOpen);
					ImGui::SameLine();
					UICommon::HelpMarker("İsteğe bağlı veri içeren animasyon olayları bu fonksiyonu tetikleyecektir.");
					if (bIsOpen) {
						ImGui::Indent();
						uint32_t i = 0;
						a_function->ForEachTrigger([&](const auto& a_trigger) {
							// draw trigger
							ImGui::TextUnformatted(a_trigger.event.data());
							if (a_trigger.payload.length() > 0) {
								ImGui::SameLine(0.f, 0.f);
								ImGui::PushStyleColor(ImGuiCol_Text, UICommon::EVENT_LOG_PAYLOAD_COLOR);
								ImGui::TextUnformatted(".");
								ImGui::SameLine(0.f, 0.f);
								UICommon::TextUnformattedEllipsis(a_trigger.payload.data());
								ImGui::PopStyleColor();
							}

							// remove trigger button
							UICommon::SecondColumn(_firstColumnWidthPercent);
							std::string buttonLabel = std::format("Tetikleyiciyi sil##{}{}", reinterpret_cast<uintptr_t>(a_function.get()), i++);
							if (ImGui::Button(buttonLabel.data())) {
								OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveTriggerJob>(a_function, a_functionSet, a_trigger);
								a_functionSet->SetDirty(true);
								a_bOutSetDirty = true;
							}
							return RE::BSVisit::BSVisitControl::kContinue;
						});

						// add new trigger
						if (a_editMode > EditMode::kNone) {
							constexpr auto popupName = "Yeni tetikleyici ekle"sv;
							std::string buttonLabel = std::format("Yeni tetikleyici ekle##{}", reinterpret_cast<uintptr_t>(a_function.get()));
							if (ImGui::Button(buttonLabel.data())) {
								const auto popupPos = ImGui::GetCursorScreenPos();
								ImGui::SetNextWindowPos(popupPos);
								ImGui::OpenPopup(popupName.data());
							}

							if (ImGui::BeginPopupModal(popupName.data(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
								static std::string eventBuffer;
								static std::string payloadBuffer;
								ImGui::InputTextWithHint("##NewTriggerEvent", "Olay adı", &eventBuffer, ImGuiInputTextFlags_CharsNoBlank);
								ImGui::SameLine();
								ImGui::TextUnformatted(".");
								ImGui::SameLine();
								ImGui::InputTextWithHint("##NewTriggerPayload", "Payload (isteğe bağlı)", &payloadBuffer, ImGuiInputTextFlags_CharsNoBlank);
								std::string addButtonLabel = std::format("Tetikleyici ekle##{}", reinterpret_cast<uintptr_t>(a_function.get()));
								ImGui::BeginDisabled(eventBuffer.empty());
								if (ImGui::Button(addButtonLabel.data())) {
									auto trigger = Functions::Trigger(eventBuffer.data(), payloadBuffer.data());
									OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::AddTriggerJob>(a_function, a_functionSet, trigger);
									a_functionSet->SetDirty(true);
									a_bOutSetDirty = true;
									eventBuffer.clear();
									payloadBuffer.clear();
									ImGui::CloseCurrentPopup();
								}
								ImGui::EndDisabled();
								ImGui::SetItemDefaultFocus();
								ImGui::SameLine();
								if (ImGui::Button("İptal")) {
									eventBuffer.clear();
									payloadBuffer.clear();
									ImGui::CloseCurrentPopup();
								}
								ImGui::EndPopup();
							}
						}

						ImGui::Unindent();
					}
				}

				ImGui::Spacing();

				ImGui::TreePop();
			}

			if (bStyleVarPushed) {
				ImGui::PopStyleVar();
			}

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();  // ImGuiStyleVar_CellPadding

		auto rectMax = ImGui::GetItemRectMax();

		auto width = ImGui::GetItemRectSize().x;
		auto groupEnd = ImGui::GetCursorPos();
		const ImGuiStyle& style = ImGui::GetStyle();
		ImVec2 invisibleButtonStart = groupEnd;
		invisibleButtonStart.y -= style.ItemSpacing.y;
		ImGui::SetCursorPos(invisibleButtonStart);
		std::string functionInvisibleDragAreaId = std::format("{}functionInvisibleDragArea", reinterpret_cast<uintptr_t>(a_function.get()));
		ImGui::InvisibleButton(functionInvisibleDragAreaId.data(), ImVec2(width, style.ItemSpacing.y));

		// Drag & Drop target - invisible button
		if (a_editMode > EditMode::kNone) {
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("DND_FUNCTION", ImGuiDragDropFlags_AcceptPeekOnly)) {
					DragFunctionPayload payload = *static_cast<DragFunctionPayload*>(imguiPayload->Data);
					// Draw our own preview of the drop because we want to draw a line below the condition

					ImDrawList* drawList = ImGui::GetWindowDrawList();
					drawList->AddLine(ImVec2(nodeRect.Min.x, rectMax.y + style.ItemSpacing.y * 0.5f), ImVec2(nodeRect.Max.x, rectMax.y + style.ItemSpacing.y * 0.5f), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 3.f);

					if (imguiPayload->IsDelivery()) {
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveFunctionJob>(payload.function, payload.functionSet, a_function, a_functionSet, true);
					}
				}
				ImGui::EndDragDropTarget();
			}
		}

		ImGui::SetCursorPos(groupEnd);

		//ImGui::EndGroup();

		return functionRect;
	}

ImRect UIMain::DrawBlankCondition(Conditions::ConditionSet* a_conditionSet, EditMode a_editMode, Conditions::ConditionType a_conditionType)
	{
		ImRect conditionRect;

		const std::string conditionTableId = std::format("{}blankConditionTable", reinterpret_cast<uintptr_t>(a_conditionSet));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
		if (ImGui::BeginTable(conditionTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			const float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;

			const std::string nodeId = std::format("Koşul yok##{}", reinterpret_cast<uintptr_t>(a_conditionSet));
			UICommon::TreeNodeCollapsedLeaf(nodeId.data(), ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);

			if (a_editMode > EditMode::kNone) {
				// Paste condition context menu
				if (ImGui::BeginPopupContextItem()) {
					// paste button
					const bool bPasteEnabled = _conditionCopy && !(a_conditionType == Conditions::ConditionType::kPreset && ConditionContainsPreset(_conditionCopy.get()));
					ImGui::BeginDisabled(!bPasteEnabled);
					if (ImGui::Button("Koşulu alta yapıştır")) {
						auto duplicate = DuplicateCondition(_conditionCopy);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::InsertConditionJob>(duplicate, a_conditionSet, nullptr);
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();
					// paste tooltip
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
						ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
						ImGui::BeginTooltip();
						bool bDummy = false;
						DrawCondition(_conditionCopy, a_conditionSet, nullptr, EditMode::kNone, Conditions::ConditionType::kNormal, nullptr, bDummy);
						ImGui::EndTooltip();
					}

					ImGui::EndPopup();
				}

				// Drag & Drop target - blank condition set
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("DND_CONDITION")) {
						DragConditionPayload payload = *static_cast<DragConditionPayload*>(imguiPayload->Data);
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveConditionJob>(payload.condition, payload.conditionSet, nullptr, a_conditionSet, true);
					}
					ImGui::EndDragDropTarget();
				}
			}

			conditionRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();  // ImGuiStyleVar_CellPadding

		return conditionRect;
	}

	ImRect UIMain::DrawBlankFunction(Functions::FunctionSet* a_functionSet, SubMod* a_parentSubMod, EditMode a_editMode, Functions::FunctionSetType a_functionSetType)
	{
		ImRect functionRect;

		auto id = reinterpret_cast<uintptr_t>(a_parentSubMod) + static_cast<uint8_t>(a_functionSetType) + reinterpret_cast<uintptr_t>(a_functionSet);
		const std::string functionTableId = std::format("{}blankFunctionTable", id);
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
		if (ImGui::BeginTable(functionTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			const float tooltipWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().WindowPadding.x * 2;

			const std::string nodeId = std::format("Fonksiyon yok##{}", id);
			UICommon::TreeNodeCollapsedLeaf(nodeId.data(), ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth);

			if (a_editMode > EditMode::kNone) {
				// Paste function context menu
				if (ImGui::BeginPopupContextItem()) {
					// paste button
					const bool bPasteEnabled = _functionCopy != nullptr;
					ImGui::BeginDisabled(!bPasteEnabled);
					if (ImGui::Button("Fonksiyonu alta yapıştır")) {
						auto duplicate = DuplicateFunction(_functionCopy);
						if (!a_functionSet) {
							a_functionSet = a_parentSubMod->CreateOrGetFunctionSet(a_functionSetType);
						}
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::InsertFunctionJob>(duplicate, a_functionSet, nullptr);
						ImGui::CloseCurrentPopup();
					}
					ImGui::EndDisabled();
					// paste tooltip
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
						ImGui::SetNextWindowSize(ImVec2(tooltipWidth, 0));
						ImGui::BeginTooltip();
						bool bDummy = false;
						DrawFunction(_functionCopy, a_functionSet, nullptr, EditMode::kNone, a_functionSetType, nullptr, bDummy);
						ImGui::EndTooltip();
					}

					ImGui::EndPopup();
				}

				// Drag & Drop target - blank function set
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* imguiPayload = ImGui::AcceptDragDropPayload("DND_FUNCTION")) {
						DragFunctionPayload payload = *static_cast<DragFunctionPayload*>(imguiPayload->Data);
						if (!a_functionSet) {
							a_functionSet = a_parentSubMod->CreateOrGetFunctionSet(a_functionSetType);
						}
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::MoveFunctionJob>(payload.function, payload.functionSet, nullptr, a_functionSet, true);
					}
					ImGui::EndDragDropTarget();
				}
			}

			functionRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();  // ImGuiStyleVar_CellPadding

		return functionRect;
	}

	bool UIMain::DrawConditionPreset(ReplacerMod* a_replacerMod, Conditions::ConditionPreset* a_conditionPreset, bool& a_bOutWasPresetRenamed)
	{
		bool bSetDirty = false;

		const std::string conditionPresetTableId = std::format("{}conditionPresetTable", reinterpret_cast<uintptr_t>(a_conditionPreset));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, UICommon::CONDITION_PRESET_BORDER_COLOR);
		if (ImGui::BeginTable(conditionPresetTableId.data(), 1, ImGuiTableFlags_BordersOuter)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);

			ImGui::PopStyleColor();  // ImGuiCol_TableBorderStrong

			const bool bNodeOpen = ImGui::TreeNodeEx(a_conditionPreset, ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth, "");

			// set dirty on all submods that have the preset
			auto setDirtyOnContainingSubMods = [&](Conditions::ConditionPreset* conditionPreset) {
				a_replacerMod->ForEachSubMod([&](SubMod* a_subMod) {
					if (ConditionSetContainsPreset(a_subMod->GetConditionSet(), conditionPreset)) {
						a_subMod->SetDirty(true);
					}
					return RE::BSVisit::BSVisitControl::kContinue;
				});
			};

			// context menu
			if (_editMode != EditMode::kNone) {
				if (ImGui::BeginPopupContextItem()) {
					// delete button
					const std::string buttonText = "Koşul ön ayarını sil";
					const auto& style = ImGui::GetStyle();
					const auto xButtonSize = ImGui::CalcTextSize(buttonText.data()).x + style.FramePadding.x * 2 + style.ItemSpacing.x;
					UICommon::ButtonWithConfirmationModal(
						buttonText, "Bu koşul ön ayarını kaldırmak istediğinizden emin misiniz?\nBu işlem geri alınamaz!\n\n"sv, [&]() {
							ImGui::ClosePopupsExceptModals();
							OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveConditionPresetJob>(a_replacerMod, a_conditionPreset->GetName());
							setDirtyOnContainingSubMods(a_conditionPreset);
							bSetDirty = true;
						},
						ImVec2(xButtonSize, 0));
					ImGui::EndPopup();
				}
			}

			// condition preset name
			ImGui::SameLine();
			if (!a_conditionPreset->IsEmpty() && a_conditionPreset->IsValid()) {
				ImGui::TextUnformatted(a_conditionPreset->GetName().data());
			} else {
				UICommon::TextUnformattedColored(UICommon::INVALID_CONDITION_COLOR, a_conditionPreset->GetName().data());
			}

			// right column, condition count text
			UICommon::SecondColumn(_firstColumnWidthPercent);
			ImGui::TextUnformatted(a_conditionPreset->NumText().data());

			if (bNodeOpen) {
				const ImGuiStyle& style = ImGui::GetStyle();

				ImGui::Spacing();
				// rename / delete condition preset
				if (_editMode != EditMode::kNone) {
					const std::string nameId = "##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_replacerMod)) + "name";
					ImGui::SetNextItemWidth(UICommon::FirstColumnWidth(_firstColumnWidthPercent));
					std::string tempName(a_conditionPreset->GetName());
					if (ImGui::InputText(nameId.data(), &tempName, ImGuiInputTextFlags_EnterReturnsTrue)) {
						if (tempName.size() > 2 && !a_replacerMod->HasConditionPreset(tempName)) {
							a_conditionPreset->SetName(tempName);
							setDirtyOnContainingSubMods(a_conditionPreset);
							a_bOutWasPresetRenamed = true;
							bSetDirty = true;
						}
					}

					UICommon::SecondColumn(_firstColumnWidthPercent);

					UICommon::ButtonWithConfirmationModal("Koşul ön ayarını sil"sv, "Koşul ön ayarını sil"sv, "Bu koşul ön ayarını kaldırmak istediğinizden emin misiniz?\nBu işlem geri alınamaz!\n\n"sv, [&]() {
						OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::RemoveConditionPresetJob>(a_replacerMod, a_conditionPreset->GetName());
						setDirtyOnContainingSubMods(a_conditionPreset);
						bSetDirty = true;
					});

					ImGui::Spacing();
				}

				// description
				const std::string descriptionId = "##" + std::to_string(reinterpret_cast<std::uintptr_t>(a_conditionPreset)) + "description";
				if (_editMode != EditMode::kNone) {
					std::string tempDescription(a_conditionPreset->GetDescription());
					ImGui::SetNextItemWidth(UICommon::FirstColumnWidth(_firstColumnWidthPercent));
					if (ImGui::InputText(descriptionId.data(), &tempDescription)) {
						a_conditionPreset->SetDescription(tempDescription);
						a_replacerMod->SetDirty(true);
					}
					UICommon::SecondColumn(_firstColumnWidthPercent);
					UICommon::TextUnformattedDisabled("Koşul ön ayar açıklaması");
					ImGui::Spacing();
				} else if (!a_conditionPreset->GetDescription().empty()) {
					UICommon::TextUnformattedWrapped(a_conditionPreset->GetDescription().data());
					ImGui::Spacing();
				}

				ImVec2 pos = ImGui::GetCursorScreenPos();
				pos.x += style.FramePadding.x;
				pos.y += style.FramePadding.y;
				ImGui::PushID(a_conditionPreset);
				if (DrawConditionSet(a_conditionPreset, nullptr, _editMode, Conditions::ConditionType::kPreset, UIManager::GetSingleton().GetRefrToEvaluate(), true, pos)) {
					bSetDirty = true;
				}
				ImGui::PopID();

				ImGui::TreePop();
			}

			ImGui::EndTable();
		}
		ImGui::PopStyleVar();  // ImGuiStyleVar_CellPadding

		return bSetDirty;
	}

	void UIMain::DrawInfoTooltip(const Info& a_info, ImGuiHoveredFlags a_flags)
	{
		if (ImGui::IsItemHovered(a_flags)) {
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8, 8 });
			if (ImGui::BeginTooltip()) {
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
				ImGui::TextUnformatted(a_info.description.data());
				if (!a_info.requiredPluginName.empty()) {
					ImGui::TextUnformatted("Gerekli eklenti:");
					ImGui::SameLine();
					UICommon::TextUnformattedColored(a_info.textColor, a_info.requiredPluginName.data());
					ImGui::SameLine();
					UICommon::TextUnformattedDisabled(a_info.requiredVersion.string("."sv).data());
					if (!a_info.requiredPluginAuthor.empty()) {
						ImGui::SameLine();
						ImGui::TextUnformatted("yazan:");
						ImGui::SameLine();
						UICommon::TextUnformattedColored(a_info.textColor, a_info.requiredPluginAuthor.data());
					}
				}
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
			ImGui::PopStyleVar();
		}
	}

	void UIMain::UnloadedAnimationsWarning()
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Yüklenmemiş davranış projelerine ait animasyonlar burada görünmeyecektir!");
		ImGui::SameLine();
		UICommon::HelpMarker("Aradığınız öğeler eksikse, bu oyun oturumunda bu öğeleri kullanan bir karakterin oyuna yüklendiğinden emin olun.");
	}

	bool UIMain::CanPreviewAnimation(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation)
	{
		if (a_refr) {
			RE::BSAnimationGraphManagerPtr graphManager = nullptr;
			a_refr->GetAnimationGraphManager(graphManager);
			if (graphManager) {
				const auto& activeGraph = graphManager->graphs[graphManager->GetRuntimeData().activeGraph];
				if (activeGraph->behaviorGraph) {
					if (const auto stringData = activeGraph->characterInstance.setup->data->stringData) {
						if (stringData->name.data() == a_replacementAnimation->GetProjectName()) {
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	bool UIMain::IsPreviewingAnimation(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, std::optional<uint16_t> a_variantIndex)
	{
		if (a_refr) {
			RE::BSAnimationGraphManagerPtr graphManager = nullptr;
			a_refr->GetAnimationGraphManager(graphManager);
			if (graphManager) {
				const auto& activeGraph = graphManager->graphs[graphManager->GetRuntimeData().activeGraph];
				if (activeGraph->behaviorGraph) {
					if (const auto activeAnimationPreview = OpenAnimationReplacer::GetSingleton().GetActiveAnimationPreview(activeGraph->behaviorGraph)) {
						if (activeAnimationPreview->GetReplacementAnimation() == a_replacementAnimation) {
							if (!a_variantIndex) {
								return true;
							}

							if (activeAnimationPreview->GetCurrentBindingIndex() == a_variantIndex) {
								return true;
							}
						}
					}
				}
			}
		}

		return false;
	}

	float UIMain::GetPreviewButtonsWidth(const ReplacementAnimation* a_replacementAnimation, bool a_bIsPreviewing)
	{
		const auto& style = ImGui::GetStyle();

		if (a_bIsPreviewing) {
			return ImGui::CalcTextSize("Durdur").x + style.FramePadding.x * 2 + style.ItemSpacing.x;
		}

		if (a_replacementAnimation->IsSynchronizedAnimation()) {
			return (ImGui::CalcTextSize("Önizleme kaynağı").x + style.FramePadding.x * 2 + style.ItemSpacing.x) + (ImGui::CalcTextSize("Önizleme hedefi").x + style.FramePadding.x * 2 + style.ItemSpacing.x);
		}

		return (ImGui::CalcTextSize("Önizleme").x + style.FramePadding.x * 2 + style.ItemSpacing.x);
	}

void UIMain::DrawPreviewButtons(RE::TESObjectREFR* a_refr, const ReplacementAnimation* a_replacementAnimation, float a_previewButtonWidth, bool a_bCanPreview, bool a_bIsPreviewing, Variant* a_variant)
	{
		const float offset = a_variant ? -10.f : 0.f;

		auto obj = a_variant ? reinterpret_cast<const void*>(a_variant) : reinterpret_cast<const void*>(a_replacementAnimation);

		if (a_bIsPreviewing) {
			ImGui::SameLine(ImGui::GetContentRegionMax().x - a_previewButtonWidth + offset);
			const std::string label = std::format("Durdur##{}", reinterpret_cast<uintptr_t>(obj));
			if (ImGui::Button(label.data())) {
				OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::StopPreviewAnimationJob>(a_refr);
			}
		} else if (a_bCanPreview) {
			ImGui::SameLine(ImGui::GetContentRegionMax().x - a_previewButtonWidth + offset);
			if (a_replacementAnimation->IsSynchronizedAnimation()) {
				const std::string sourceLabel = std::format("Önizleme kaynağı##{}", reinterpret_cast<uintptr_t>(obj));
				if (ImGui::Button(sourceLabel.data())) {
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::BeginPreviewAnimationJob>(a_refr, a_replacementAnimation, Settings::synchronizedClipSourcePrefix, a_variant);
				}
				ImGui::SameLine();
				const std::string targetLabel = std::format("Önizleme hedefi##{}", reinterpret_cast<uintptr_t>(obj));
				if (ImGui::Button(targetLabel.data())) {
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::BeginPreviewAnimationJob>(a_refr, a_replacementAnimation, Settings::synchronizedClipTargetPrefix, a_variant);
				}
			} else {
				const std::string label = std::format("Önizleme##{}", reinterpret_cast<uintptr_t>(obj));
				if (ImGui::Button(label.data())) {
					OpenAnimationReplacer::GetSingleton().QueueJob<Jobs::BeginPreviewAnimationJob>(a_refr, a_replacementAnimation, a_variant);
				}
			}
		}
	}

	int UIMain::ReferenceInputTextCallback(struct ImGuiInputTextCallbackData* a_data)
	{
		RE::FormID formID;
		auto [ptr, ec]{ std::from_chars(a_data->Buf, a_data->Buf + a_data->BufTextLen, formID, 16) };
		if (ec == std::errc()) {
			UIManager::GetSingleton().SetRefrToEvaluate(RE::TESForm::LookupByID<RE::TESObjectREFR>(formID));
		} else {
			UIManager::GetSingleton().SetRefrToEvaluate(nullptr);
		}

		return 0;
	}

	bool UIMain::BeginDragDropSourceEx(ImGuiDragDropFlags a_flags /*= 0*/, ImVec2 a_tooltipSize /*= ImVec2(0, 0)*/)
	{
		using namespace ImGui;

		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;

		// FIXME-DRAGDROP: While in the common-most "drag from non-zero active id" case we can tell the mouse button,
		// in both SourceExtern and id==0 cases we may requires something else (explicit flags or some heuristic).
		ImGuiMouseButton mouse_button = ImGuiMouseButton_Left;

		bool source_drag_active = false;
		ImGuiID source_id = 0;
		ImGuiID source_parent_id = 0;
		if (!(a_flags & ImGuiDragDropFlags_SourceExtern)) {
			source_id = g.LastItemData.ID;
			if (source_id != 0) {
				// Common path: items with ID
				if (g.ActiveId != source_id)
					return false;
				if (g.ActiveIdMouseButton != -1)
					mouse_button = g.ActiveIdMouseButton;
				if (g.IO.MouseDown[mouse_button] == false || window->SkipItems)
					return false;
				g.ActiveIdAllowOverlap = false;
			} else {
				// Uncommon path: items without ID
				if (g.IO.MouseDown[mouse_button] == false || window->SkipItems)
					return false;
				if ((g.LastItemData.StatusFlags & ImGuiItemStatusFlags_HoveredRect) == 0 && (g.ActiveId == 0 || g.ActiveIdWindow != window))
					return false;

				// If you want to use BeginDragDropSource() on an item with no unique identifier for interaction, such as Text() or Image(), you need to:
				// A) Read the explanation below, B) Use the ImGuiDragDropFlags_SourceAllowNullID flag.
				if (!(a_flags & ImGuiDragDropFlags_SourceAllowNullID)) {
					IM_ASSERT(0);
					return false;
				}

				// Magic fallback to handle items with no assigned ID, e.g. Text(), Image()
				// We build a throwaway ID based on current ID stack + relative AABB of items in window.
				// THE IDENTIFIER WON'T SURVIVE ANY REPOSITIONING/RESIZING OF THE WIDGET, so if your widget moves your dragging operation will be canceled.
				// We don't need to maintain/call ClearActiveID() as releasing the button will early out this function and trigger !ActiveIdIsAlive.
				// Rely on keeping other window->LastItemXXX fields intact.
				source_id = g.LastItemData.ID = window->GetIDFromRectangle(g.LastItemData.Rect);
				KeepAliveID(source_id);
				const bool is_hovered = ItemHoverable(g.LastItemData.Rect, source_id);
				if (is_hovered && g.IO.MouseClicked[mouse_button]) {
					SetActiveID(source_id, window);
					FocusWindow(window);
				}
				if (g.ActiveId == source_id)  // Allow the underlying widget to display/return hovered during the mouse release frame, else we would get a flicker.
					g.ActiveIdAllowOverlap = is_hovered;
			}
			if (g.ActiveId != source_id)
				return false;
			source_parent_id = window->IDStack.back();
			source_drag_active = IsMouseDragging(mouse_button);

			// Disable navigation and key inputs while dragging + cancel existing request if any
			SetActiveIdUsingAllKeyboardKeys();
		} else {
			window = nullptr;
			source_id = ImHashStr("#SourceExtern");
			source_drag_active = true;
		}

		if (source_drag_active) {
			if (!g.DragDropActive) {
				IM_ASSERT(source_id != 0);
				ClearDragDrop();
				ImGuiPayload& payload = g.DragDropPayload;
				payload.SourceId = source_id;
				payload.SourceParentId = source_parent_id;
				g.DragDropActive = true;
				g.DragDropSourceFlags = a_flags;
				g.DragDropMouseButton = mouse_button;
				if (payload.SourceId == g.ActiveId)
					g.ActiveIdNoClearOnFocusLoss = true;
			}
			g.DragDropSourceFrameCount = g.FrameCount;
			g.DragDropWithinSource = true;

			if (!(a_flags & ImGuiDragDropFlags_SourceNoPreviewTooltip)) {
				// Target can request the Source to not display its tooltip (we use a dedicated flag to make this request explicit)
				// We unfortunately can't just modify the source flags and skip the call to BeginTooltip, as caller may be emitting contents.
				SetNextWindowSize(a_tooltipSize);  // <--- ADDED
				BeginTooltipEx(ImGuiTooltipFlags_None, ImGuiWindowFlags_None);
				if (g.DragDropAcceptIdPrev && (g.DragDropAcceptFlags & ImGuiDragDropFlags_AcceptNoPreviewTooltip)) {
					ImGuiWindow* tooltip_window = g.CurrentWindow;
					tooltip_window->Hidden = tooltip_window->SkipItems = true;
					tooltip_window->HiddenFramesCanSkipItems = 1;
				}
			}

			if (!(a_flags & ImGuiDragDropFlags_SourceNoDisableHover) && !(a_flags & ImGuiDragDropFlags_SourceExtern))
				g.LastItemData.StatusFlags &= ~ImGuiItemStatusFlags_HoveredRect;

			return true;
		}
		return false;
	}

	bool UIMain::ConditionContainsPreset(Conditions::ICondition* a_condition, Conditions::ConditionPreset* a_conditionPreset) const
	{
		if (a_condition == nullptr) {
			return false;
		}

		//if (a_condition->GetConditionType() == Conditions::ConditionType::kPreset) {
		//	return true;
		//}

		if (const auto numComponents = a_condition->GetNumComponents(); numComponents > 0) {
			for (uint32_t i = 0; i < numComponents; i++) {
				const auto component = a_condition->GetComponent(i);
				if (component->GetType() == Conditions::ConditionComponentType::kPreset) {
					if (a_conditionPreset == nullptr) {
						return true;
					}
					// check if equals the given condition preset
					const auto conditionPresetComponent = static_cast<Conditions::ConditionPresetComponent*>(component);
					if (conditionPresetComponent->conditionPreset == a_conditionPreset) {
						return true;
					}
				}

				if (component->GetType() == Conditions::ConditionComponentType::kMulti) {
					const auto multiConditionComponent = static_cast<Conditions::IMultiConditionComponent*>(component);
					if (const auto conditionSet = multiConditionComponent->GetConditions()) {
						if (ConditionSetContainsPreset(conditionSet, a_conditionPreset)) {
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	bool UIMain::ConditionSetContainsPreset(Conditions::ConditionSet* a_conditionSet, Conditions::ConditionPreset* a_conditionPreset) const
	{
		if (a_conditionSet == nullptr) {
			return false;
		}

		const auto result = a_conditionSet->ForEach([&](std::unique_ptr<Conditions::ICondition>& a_condition) {
			if (ConditionContainsPreset(a_condition.get(), a_conditionPreset)) {
				return RE::BSVisit::BSVisitControl::kStop;
			}
			return RE::BSVisit::BSVisitControl::kContinue;
		});

		return result == RE::BSVisit::BSVisitControl::kStop;
	}
}
